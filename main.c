/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/

#define EDITOR_VERSION "0.0.1"

// ctrl-key macro to bitwise-AND the key with 0x1f (00011111) to get the ASCII value of the ctrl-key
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey
{
    ARROW_LEFT = '1000',
    ARROW_RIGHT, // 1001
    ARROW_UP,    // 1002
    ARROW_DOWN,  // 1003
    HOME_KEY,    // 1004
    END_KEY,     // 1005
    PAGE_UP,     // 1006
    PAGE_DOWN,   // 1007
};

/*** data ***/

struct editorConfig
{
    int cx, cy;

    int screenrows;
    int screencols;

    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

// prints an error message and exits the program with status 1
void die(const char *s)
{
    // [2J - clear entire screen
    write(STDOUT_FILENO, "\x1b[2J", 4);

    // H - position cursor
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// disables raw mode and enables canonical mode
void disableRawMode()
{
    // save the original terminal attributes to the orig_termios struct to apply them after the program ends
    // TCSAFLUSH - apply the change immediately and discard any input that hasn't been read
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

// disables canonical | cooked mode (keyboard input is only sent to the program after pressing [enter]) and
// enables raw mode (keyboard input is sent to the program immediately byte-by-byte)
void enableRawMode()
{
    // read the current terminal attributes into orig_termios struct
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);

    // create a raw struct and copy the orig_termios struct into it modifying the flags
    struct termios raw = E.orig_termios;

    // iflag - input flags -> bitwise-NOT to disable the flags
    // BRKINT - break condition
    // ICRNL - carriage return
    // INPCK - parity check
    // ISTRIP - strip 8th bit
    // IXON - ctrl-s and ctrl-q
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // oflag - output flags -> bitwise-NOT to disable the flags
    // OPOST - output processing
    raw.c_oflag &= ~(OPOST);

    // cflag - control flags -> bitwise-OR to enable the flags
    // CS8 - character size 8 bits per byte
    raw.c_cflag |= (CS8);

    // lflag - local | misc flags -> bitwise-NOT to disable the flags
    // ECHO - echo input characters
    // ICANON - canonical mode
    // IEXTEN - enable ctrl-v and ctrl-o
    // ISIG - enable ctrl-c and ctrl-z
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // c_cc - control characters
    // VMIN - minimum number of bytes of input needed before read() can return
    // VTIME - maximum amount of time to wait before read() returns
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // set the terminal attributes to the raw struct
    // TCSAFLUSH - apply the change immediately and discard any input that hasn't been read
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// wait a keypress and return it
int editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b')
    {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }

        return '\x1b';
    }
    else
    {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    // escape sequence to query the cursor position
    // [6n - query cursor position
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    // read the response from the terminal
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;

    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    // ioctl - input/output control
    // TIOCGWINSZ - get window size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        // escape sequence to position the cursor at the bottom-right corner of the screen
        // [999C - move cursor 999 characters to the right
        // [999B - move cursor 999 characters down

        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;

        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

struct abuf
{
    char *b;
    int len;
};

// buffer constructor
#define ABUF_INIT \
    {             \
        NULL, 0   \
    }

// appends a string to the buffer
void abAppend(struct abuf *ab, const char *s, int len)
{
    // reallocates the buffer memory to fit the new string
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;

    // copies the new string to the end of the buffer
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab)
{
    for (int y = 0; y < E.screenrows; y++)
    {
        if (y == E.screenrows / 3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "simple text editor -- version %s", EDITOR_VERSION);
            if (welcomelen > E.screencols)
                welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--)
                abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        }
        else
        {
            abAppend(ab, "~", 1);
        }

        // [K - erase in line
        abAppend(ab, "\x1b[K", 3);

        if (y < E.screenrows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;

    // [?25l - hide cursor
    abAppend(&ab, "\x1b[?25l", 6);

    // H - position cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

// moves the cursor based on the keypress
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1)
            E.cx++;
        break;
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenrows - 1)
            E.cy++;
        break;
    }
}

// waits for a keypress and handles it
void editorProcessKeypress()
{
    int c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

    case HOME_KEY:
        E.cx = 0;
        break;
        
    case END_KEY:
        E.cx = E.screencols - 1;
        break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/*** init ***/

void initEditor()
{
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main()
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

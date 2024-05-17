/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/*** defines ***/

#define EDITOR_VERSION "0.0.1"
#define EDITOR_TAB_STOP 8

// ctrl-key macro to bitwise-AND the key with 0x1f (00011111) to get the ASCII value of the ctrl-key
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey
{
    ARROW_LEFT = '1000',
    ARROW_RIGHT, // 1001
    ARROW_UP,    // 1002
    ARROW_DOWN,  // 1003
    DEL_KEY,     // 1004
    HOME_KEY,    // 1005
    END_KEY,     // 1006
    PAGE_UP,     // 1007
    PAGE_DOWN,   // 1008
};

/*** data ***/

typedef struct editorRow
{
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;

struct editorConfig
{
    int cx, cy;
    int rx;

    int rowoff;
    int coloff;

    int screenrows;
    int screencols;

    int numrows;
    erow *row;

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
                    case '3':
                        return DEL_KEY;
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

/*** row operations ***/

// converts the index of a character in a row from the cx (character index) to the rx (render index)
int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;

    for (int j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (EDITOR_TAB_STOP - 1) - (rx % EDITOR_TAB_STOP);
        rx++;
    }
    return rx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
            tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs * (EDITOR_TAB_STOP - 1) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % EDITOR_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;

    editorUpdateRow(&E.row[at]);

    E.numrows++;
}

/*** file i/o ***/

// opens a file and reads it line by line appending each line to the editor
void editorOpen(char *filename)
{
    FILE *fp = fopen(filename, "r");

    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
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

// scrolls the editor to keep the cursor in view
void editorScroll()
{
    E.rx = 0;
    if (E.cy < E.numrows)
    {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows)
    {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff)
    {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols)
    {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab)
{
    for (int y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;

        if (filerow >= E.numrows)
        {
            if (E.numrows == 0 && y == E.screenrows / 3)
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
        }
        // display the content of the file
        else
        {
            int len = E.row[filerow].size - E.coloff;

            if (len < 0)
                len = 0;

            if (len > E.screencols)
                len = E.screencols;

            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }

        // [K - erase in line
        abAppend(ab, "\x1b[K", 3);

        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab)
{
    // [7m - invert colors
    abAppend(ab, "\x1b[7m", 4);

    int len = 0;
    while (len < E.screencols)
    {
        abAppend(ab, "", 1);
        len++;
    }

    // [m - reset colors
    abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen()
{
    editorScroll();

    struct abuf ab = ABUF_INIT;

    // [?25l - hide cursor
    abAppend(&ab, "\x1b[?25l", 6);

    // [H - position cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

// moves the cursor based on the keypress
void editorMoveCursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size)
            E.cx++;
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows)
            E.cy++;
        break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;

    if (E.cx > rowlen)
        E.cx = rowlen;
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
        if (E.cy < E.numrows)
            E.cx = E.row[E.cy].size;
        break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
        if (c == PAGE_UP)
            E.cy = E.rowoff;
        else if (c == PAGE_DOWN)
        {
            E.cy = E.rowoff + E.screenrows - 1;
            if (E.cy > E.numrows)
                E.cy = E.numrows;
        }

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
    E.rx = 0;

    E.rowoff = 0;
    E.coloff = 0;

    E.numrows = 0;

    E.row = NULL;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");

    E.screenrows -= 1;
}

int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

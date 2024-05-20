/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/*** defines ***/

#define EDITOR_VERSION "0.0.1"
#define EDITOR_TAB_STOP 8
#define EDITOR_QUIT_TIMES 3

// ctrl-key macro to bitwise-AND the key with 0x1f (00011111) to get the ASCII value of the ctrl-key
#define CTRL_KEY(k) ((k) & 0x1f)

/**
 * @brief Enumeration representing the different keys used in the editor.
 *
 * This enumeration defines the different keys that can be used in the editor.
 * Each key is represented by a unique value.
 */
enum editorKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
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

/**
 * @struct editorRow
 * @brief Represents a row in the text editor.
 *
 * This struct contains information about the size of the row, the rendered size,
 * and pointers to the character array and the rendered version of the row.
 */
typedef struct editorRow
{
    int size;     ///< The size of the row.
    int rsize;    ///< The rendered size of the row.
    char *chars;  ///< Pointer to the character array of the row.
    char *render; ///< Pointer to the rendered version of the row.
} erow;

/**
 * @struct editorConfig
 * @brief Represents the configuration of the text editor.
 *
 * The editorConfig struct stores various properties and settings related to the text editor.
 * It includes information about the cursor position, screen dimensions, number of rows, modified status,
 * filename, status message, and terminal settings.
 */
struct editorConfig
{
    int cx, cy; /**< The x and y coordinates of the cursor. */
    int rx;     /**< The index of the cursor in the rendered row. */

    int rowoff; /**< The offset of the displayed rows. */
    int coloff; /**< The offset of the displayed columns. */

    int screenrows; /**< The number of rows in the terminal screen. */
    int screencols; /**< The number of columns in the terminal screen. */

    int numrows; /**< The total number of rows in the text buffer. */
    erow *row;   /**< An array of erow structs representing each row of the text buffer. */

    int modified; /**< Flag indicating if the text buffer has been modified. */

    char *filename; /**< The name of the file being edited. */

    char statusmsg[80];    /**< The status message to be displayed in the editor. */
    time_t statusmsg_time; /**< The time at which the status message was set. */

    struct termios orig_termios; /**< The original terminal settings. */
};

struct editorConfig E;

/*** prototypes ***/

/**
 * Sets the status message of the editor.
 *
 * @param fmt The format string for the status message.
 * @param ... Additional arguments to be formatted into the status message.
 * @return None
 */
void editorSetStatusMessage(const char *fmt, ...);

/**
 * Refreshes the screen of the text editor.
 *
 * @param None
 * @return None
 */
void editorRefreshScreen();

/**
 * Prompts the user for input with the given prompt message and returns the user's input.
 *
 * @param prompt The message to display as the prompt.
 * @param callback A pointer to a function that will be called with the user's input and its length.
 * @return The user's input as a string.
 */
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/*** terminal ***/

/**
 * Prints an error message and exits the program with status 1.
 *
 * @param s The error message to be printed.
 *
 */
void die(const char *s)
{
    // [2J - clear entire screen
    write(STDOUT_FILENO, "\x1b[2J", 4);

    // H - position cursor
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

/**
 * Disables raw mode and enables canonical mode.
 *
 * This function is responsible for disabling raw mode and enabling canonical mode
 * in the terminal. It saves the original terminal attributes to the `orig_termios`
 * struct to apply them after the program ends. The `TCSAFLUSH` flag is used to apply
 * the change immediately and discard any input that hasn't been read.
 *
 * @param None
 * @return None
 */
void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/**
 * Disables canonical mode (keyboard input is only sent to the program after pressing [enter])
 * and enables raw mode (keyboard input is sent to the program immediately byte-by-byte).
 *
 * This function reads the current terminal attributes into the `orig_termios` struct,
 * creates a raw struct and copies the `orig_termios` struct into it, modifying the flags
 * to enable raw mode. It also sets the minimum number of bytes of input needed before `read()`
 * can return to 0 and the maximum amount of time to wait before `read()` returns to 1.
 *
 * @param None
 * @return None
 */
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

/**
 * Reads a keypress from the user and returns the corresponding key code.
 *
 * @param None
 * @return The key code of the pressed key.
 */
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

/**
 * Retrieves the current cursor position in the terminal.
 *
 * @param rows Pointer to an integer variable to store the row position of the cursor.
 * @param cols Pointer to an integer variable to store the column position of the cursor.
 * @return 0 if successful, -1 otherwise.
 */
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

/**
 * Retrieves the size of the terminal window.
 *
 * This function uses the `ioctl` system call to get the window size of the terminal.
 * If the `ioctl` call fails or the window size is 0, it uses an escape sequence to position
 * the cursor at the bottom-right corner of the screen and then calls `getCursorPosition`
 * to get the actual window size.
 *
 * @param rows Pointer to an integer to store the number of rows in the window.
 * @param cols Pointer to an integer to store the number of columns in the window.
 * @return 0 if successful, -1 if an error occurred.
 */
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

/**
 * Converts the index of a character in a row from the cx (character index) to the rx (render index).
 *
 * @param row The row containing the characters.
 * @param cx The character index in the row.
 * @return The render index of the character.
 */
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

/**
 * Converts a visual column index (rx) to a character index (cx) for a given row.
 *
 * @param row The row for which to convert the visual column index.
 * @param rx The visual column index to convert.
 * @return The character index (cx) corresponding to the given visual column index (rx).
 */
int editorRowRxToCx(erow *row, int rx)
{
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->size; cx++)
    {
        if (row->chars[cx] == '\t')
            cur_rx += (EDITOR_TAB_STOP - 1) - (cur_rx % EDITOR_TAB_STOP);

        cur_rx++;

        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

/**
 * Updates the render field of a row.
 *
 * This function updates the render field of a given row by converting tabs
 * into spaces and allocating memory for the updated render string.
 *
 * @param row A pointer to the row structure to be updated.
 * @return None
 */
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

/**
 * Inserts a new row at the specified position in the editor.
 *
 * @param at The index at which to insert the row.
 * @param s The string to be inserted.
 * @param len The length of the string.
 * @return None
 */
void editorInsertRow(int at, char *s, size_t len)
{
    if (at < 0 || at > E.numrows)
        return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;

    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.modified = 1;
}

/**
 * Frees the memory allocated for a row in the editor.
 *
 * @param row The row to be freed.
 * @return None
 */
void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
}

/**
 * Deletes a row from the editor.
 *
 * @param at The index of the row to delete.
 * @return None
 */
void editorDelRow(int at)
{
    if (at < 0 || at >= E.numrows)
        return;

    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.modified = 1;
}

/**
 * Inserts a character at a given index in a row.
 *
 * @param row The row in which the character will be inserted.
 * @param at The index at which the character will be inserted.
 * @param c The character to be inserted.
 * @return None
 */
void editorRowInsertChar(erow *row, int at, int c)
{
    if (at < 0 || at > row->size)
        at = row->size;

    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;

    row->chars[at] = c;
    editorUpdateRow(row);
    E.modified = 1;
}

/**
 * Inserts a new line in the text editor at the current cursor position.
 * If the cursor is at the beginning of a line, a new empty line is inserted.
 * If the cursor is in the middle of a line, the text after the cursor is moved to a new line.
 * The cursor is then moved to the beginning of the new line.
 *
 * @param None
 * @return None
 */
void editorInsertNewLine()
{
    if (E.cx == 0)
    {
        editorInsertRow(E.cy, "", 0);
    }
    else
    {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

/**
 * Deletes a character at a given index in a row.
 *
 * @param row The row from which to delete the character.
 * @param at The index of the character to delete.
 * @return None
 */
void editorRowDelChar(erow *row, int at)
{
    if (at < 0 || at >= row->size)
        return;

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;

    editorUpdateRow(row);
    E.modified = 1;
}

/*** editor operations ***/

/**
 * Inserts a character at the cursor position.
 * If the cursor is at the end of the file, a new empty row is appended before inserting the character.
 *
 * @param c The character to be inserted.
 * @return None
 */
void editorInsertChar(int c)
{
    if (E.cy == E.numrows)
        editorInsertRow(E.numrows, "", 0);

    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

/**
 * Appends a string to the end of a row.
 *
 * This function appends the given string `s` of length `len` to the end of the specified row.
 * It reallocates memory for the row's characters, copies the string to the allocated memory,
 * updates the row's size, adds a null terminator at the end, updates the editor, and marks
 * the editor as modified.
 *
 * @param row The row to append the string to.
 * @param s The string to append.
 * @param len The length of the string.
 * @return None
 */
void editorRowAppendString(erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.modified = 1;
}

/**
 * Deletes a character at the cursor position.
 * If the cursor is at the end of the file, nothing happens.
 * If the cursor is not at the beginning of the line, the character to the left of the cursor is deleted.
 * After deleting the character, the cursor is moved one position to the left.
 *
 * @param None
 * @return None
 */
void editorDelChar()
{
    if (E.cy == E.numrows)
        return;
    if (E.cx == 0 && E.cy == 0)
        return;
    erow *row = &E.row[E.cy];
    if (E.cx > 0)
    {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else
    {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

/*** file i/o ***/

/**
 * Converts the editor rows to a single string.
 *
 * This function takes the editor rows and concatenates them into a single string,
 * separating each row with a newline character. The resulting string is stored in
 * a dynamically allocated buffer, and the length of the buffer is returned through
 * the `buflen` parameter.
 *
 * @param buflen A pointer to an integer that will store the length of the resulting string.
 * @return A pointer to the dynamically allocated buffer containing the converted string.
 */
char *editorRowsToString(int *buflen)
{
    int totlen = 0;
    int j;

    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;

    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;

    for (j = 0; j < E.numrows; j++)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

/**
 * Opens a file and reads it line by line, appending each line to the editor.
 *
 * @param filename The name of the file to be opened.
 * @return None
 */
void editorOpen(char *filename)
{
    free(E.filename);
    E.filename = strdup(filename);

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

        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.modified = 0;
}

/**
 * Saves the contents of the editor buffer to a file.
 * If the file doesn't exist, it will be created.
 *
 * @param None
 * @return None
 */
void editorSave()
{
    if (E.filename == NULL)
    {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL)
        {
            editorSetStatusMessage("Operation aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowsToString(&len);

    // O_RDWR - read/write mode
    // O_CREAT - create the file if it doesn't exist
    // 0644 - file permissions
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.modified = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find ***/

/**
 * Callback function for finding text in the editor.
 *
 * This function is called when the user wants to find a specific text in the editor.
 * It searches for the given query in each row of the editor's content and moves the cursor
 * to the first occurrence of the query. If the user presses the enter key or the escape key,
 * the function returns without performing any action.
 *
 * @param query The text to search for.
 * @param key The key that triggered the callback.
 * @return None
 */
void editorFindCallback(char *query, int key)
{
    static int last_match = -1;
    static int direction = 1;

    if (key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP)
    {
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1)
        direction = 1;

    int current = last_match;

    for (int i = 0; i < E.numrows; i++)
    {
        current += direction;
        if (current == -1)
            current = E.numrows - 1;
        else if (current == E.numrows)
            current = 0;

        erow *row = &E.row[current];

        char *match = strstr(row->render, query);
        if (match)
        {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            break;
        }
    }
}

/**
 * Searches for a given query in the text editor.
 *
 * This function prompts the user to enter a search query and searches for that query in the text editor.
 * If the user cancels the search by pressing ESC, the function restores the editor's previous state.
 *
 * @param None
 * @return None
 */
void editorFind()
{
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s (ESC | Arrows | Enter)", editorFindCallback);

    if (query)
        free(query);
    else
    {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
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

/**
 * Appends a string to the buffer.
 *
 * This function reallocates the buffer memory to fit the new string and then
 * copies the new string to the end of the buffer.
 *
 * @param ab The buffer to append the string to.
 * @param s The string to append.
 * @param len The length of the string to append.
 * @return None
 */
void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/**
 * Frees the memory allocated for the abuf structure.
 *
 * @param ab The abuf structure to be freed.
 * @return None
 */
void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*** output ***/

/**
 * Scrolls the editor to keep the cursor in view.
 *
 * @param None
 * @return None
 */
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

/**
 * Draws the rows of the editor on the screen.
 *
 * @param ab The buffer to append the output to.
 * @return None
 */
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

/**
 * Draws the status bar on the screen.
 *
 * @param ab The buffer to append the status bar content to.
 * @return None
 */
void editorDrawStatusBar(struct abuf *ab)
{
    // [7m - invert colors
    abAppend(ab, "\x1b[7m", 4);

    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.modified ? "(modified)" : "");

    int rlen = snprintf(rstatus, sizeof(rstatus), "%d / %d", E.cy + 1, E.numrows);

    if (len > E.screencols)
        len = E.screencols;

    abAppend(ab, status, len);

    while (len < E.screencols)
    {
        if (E.screencols - len == rlen)
        {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else
        {
            abAppend(ab, "", 1);
            len++;
        }
    }

    // [m - reset colors
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

/**
 * Draws the message bar on the screen.
 *
 * @param ab The output buffer to append the message bar to.
 * @return None
 */
void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);

    if (msglen > E.screencols)
        msglen = E.screencols;

    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

/**
 * Refreshes the screen by redrawing all the elements of the text editor.
 *
 * @param None
 * @return None
 */
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
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/**
 * Sets the status message of the editor.
 *
 * This function takes a format string and a variable number of arguments, similar to the `printf` function.
 * It formats the message according to the format string and stores it in the `E.statusmsg` buffer.
 * The formatted message is truncated if it exceeds the size of the buffer.
 * The current time is also stored in `E.statusmsg_time` to track when the status message was set.
 *
 * @param fmt The format string for the status message.
 * @param ... The variable number of arguments to be formatted.
 * @return None
 */
void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/

/**
 * Prompts the user for input and returns the entered text.
 *
 * This function displays a prompt message to the user and waits for the user to enter text.
 * The entered text is stored in a dynamically allocated string and returned to the caller.
 * The function also accepts an optional callback function that can be used to perform additional actions on the entered text.
 *
 * @param prompt The prompt message to display to the user.
 * @param callback A pointer to a function that takes a char pointer and an integer as arguments.
 *                 This function will be called with the entered text and the last key pressed as arguments.
 *                 Pass NULL if no callback function is needed.
 * @return The entered text as a dynamically allocated string.
 *         Returns NULL if the user cancels the input by pressing the escape key.
 */
char *editorPrompt(char *prompt, void (*callback)(char *, int))
{
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';
    while (1)
    {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if (buflen != 0)
                buf[--buflen] = '\0';
        }
        else if (c == '\x1b')
        {
            editorSetStatusMessage("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        }
        else if (c == '\r')
        {
            if (buflen != 0)
            {
                editorSetStatusMessage("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        }
        else if (!iscntrl(c) && c < 128)
        {
            if (buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback)
            callback(buf, c);
    }
}

/**
 * Moves the cursor based on the keypress.
 *
 * @param key The key that was pressed.
 * @return None
 */
void editorMoveCursor(int key)
{
    // Get the current row
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key)
    {
    case ARROW_LEFT:
        // Move cursor left if not at the beginning of the line
        if (E.cx != 0)
            E.cx--;
        // Move cursor to the end of the previous line if at the beginning of the current line
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        // Move cursor right if not at the end of the line
        if (row && E.cx < row->size)
            E.cx++;
        // Move cursor to the beginning of the next line if at the end of the current line
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        // Move cursor up if not at the top row
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        // Move cursor down if not at the bottom row
        if (E.cy < E.numrows)
            E.cy++;
        break;
    }

    // Update the current row and its length
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;

    // Adjust the cursor position if it exceeds the row length
    if (E.cx > rowlen)
        E.cx = rowlen;
}

/**
 * Waits for a keypress and handles it.
 *
 * @param None
 * @return None
 */
void editorProcessKeypress()
{
    static int quit_times = EDITOR_QUIT_TIMES;

    int c = editorReadKey();

    switch (c)
    {
    case '\r':
        editorInsertNewLine();
        break;

    case CTRL_KEY('q'):
        if (E.modified && quit_times > 0)
        {
            editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                                   "Press Ctrl-Q %d more times to quit.",
                                   quit_times);
            quit_times--;
            return;
        }

        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

    case CTRL_KEY('s'):
        editorSave();
        break;

    case HOME_KEY:
        E.cx = 0;
        break;

    case END_KEY:
        if (E.cy < E.numrows)
            E.cx = E.row[E.cy].size;
        break;

    case CTRL_KEY('f'):
        editorFind();
        break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if (c == DEL_KEY)
            editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
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

    case CTRL_KEY('l'):
    case '\x1b':
        break;

    default:
        editorInsertChar(c);
        break;
    }

    quit_times = EDITOR_QUIT_TIMES;
}

/*** init ***/

/**
 * Initializes the editor by setting the initial values for various editor properties.
 *
 * @param None
 * @return None
 */
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;

    E.rowoff = 0;
    E.coloff = 0;

    E.numrows = 0;

    E.row = NULL;

    E.modified = 0;

    E.filename = NULL;

    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");

    E.screenrows -= 2;
}

int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = search");

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/

// ctrl-key macro to bitwise-AND the key with 0x1f (00011111) to get the ASCII value of the ctrl-key
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

// prints an error message and exits the program with status 1
void die(const char *s)
{
    perror(s);
    exit(1);
}

// disables raw mode and enables canonical mode
void disableRawMode()
{
    // save the original terminal attributes to the orig_termios struct to apply them after the program ends
    // TCSAFLUSH - apply the change immediately and discard any input that hasn't been read
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

// disables canonical | cooked mode (keyboard input is only sent to the program after pressing [enter]) and enables raw mode
void enableRawMode()
{
    // read the current terminal attributes into orig_termios struct
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);

    // create a raw struct and copy the orig_termios struct into it modifying the flags
    struct termios raw = orig_termios;

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

/*** init ***/

int main()
{
    enableRawMode();

    while (1)
    {
        char c = '\0';

        // read a character from the standard input
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");

        // iscntrl - checks if the character is a control character
        if (iscntrl(c))
        {
            // print the ASCII value of the character
            printf("%d\r\n", c);
        }
        else
        {
            // print the ASCII value and the character
            printf("%d ('%c')\r\n", c, c);
        }

        // if the character is the ctrl-q key, break the loop
        if (c == CTRL_KEY('q'))
            break;
    }

    return 0;
}

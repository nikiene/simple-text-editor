#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

struct termios orig_termios;

// disables raw mode and enables canonical mode
void disableRawMode()
{
    // save the original terminal attributes to the orig_termios struct to apply them after the program ends
    // TCSAFLUSH - apply the change immediately and discard any input that hasn't been read
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// disables canonical | cooked mode (keyboard input is only sent to the program after pressing [enter]) and enables raw mode
void enableRawMode()
{
    // read the current terminal attributes into orig_termios struct
    tcgetattr(STDIN_FILENO, &orig_termios);
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

    // set the terminal attributes to the raw struct
    // TCSAFLUSH - apply the change immediately and discard any input that hasn't been read
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
    enableRawMode();

    char c;

    // read a byte from the standard input and store it in the variable 'c'
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    {
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
    }

    return 0;
}

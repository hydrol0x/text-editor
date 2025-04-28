#include <ctype.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdio.h>
#include <termios.h>
#include <errno.h>

struct termios orig_termios;

void die(const char *s) {
  perror(s);
  exit(1);
}
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) 
  {
    die("tcsetattr");
  };
}

void enableRawMode() {
  
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  // turn off output processing like \n -> \r\n
  raw.c_oflag &= ~(OPOST);
  // IXON turn off ctrl-S and ctrl-Q suspension 
  //  ICRNL do not subsitute carriage return (13) for new line (10)
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP ); 
  // turn off echo in term = term no longer prints user input
  // turn off ICANON = read by byte not read by line
  // turn off ISIG = ctrl+C ctrl+Z return the byte sequence
  // turn off IEXTEN = ctrl+V escape is turned off 
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); 
  raw.c_cflag |= ~(CS8);

  raw.c_cc[VMIN] = 0; // set the minimum number of bytes that read() can read and return
  raw.c_cc[VTIME] = 1; // set the timeout time in 1/10 sec of read()

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int main() { 
  enableRawMode();
  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");// read in 1 byte into c from stdin
    if (iscntrl(c)) {
      printf("%d\r\n",c);
    } else {
      printf("%d ('%c')", c,c);
    }
    if (c=='q') break;

  }
  return 0;
}

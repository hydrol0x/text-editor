#include <asm-generic/ioctls.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#define VERSION_NUMBER "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN, 
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
};

//
// --- append buffer --
//

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  // allocate a new bloc starting at *b and with a length equal to old length + length of new string
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len); // copy contents of string s to the latter half of the new allocated memory block
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;


void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) 
  {
    die("tcsetattr");
  };
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
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

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c=='\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } 
      else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

void editorDrawsRows(struct abuf *ab) {
  int y;
  for (y=0; y<E.screenrows; y++) {
    if (y == E.screenrows/3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),"Kilo editor -- version %s", VERSION_NUMBER); 
      if (welcomelen > E.screencols) welcomelen = E.screencols;
    
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }
    

    abAppend(ab, "\x1b[K", 3); // clear the line to the right of the cursor
    if (y<E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // hide cursor
  abAppend(&ab, "\x1b[H", 3); // move cursor to top left
  // abAppend(&ab, "\x1b[2J", 4); // clear screen

  editorDrawsRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+1, E.cx+1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // show cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorMoveCursor(int key) {
  // todo try to do a vim mode
  switch (key) {
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy ++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy --;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx ++;
      }
      break;
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx -= 1;
      }
      break;
  }
}
void editorProcessKeypress() {
  int c = editorReadKey();

  switch(c) {
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


int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i]='\0';
  if (buf[0] != '\x1b' || buf[1] != '[' ) return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1; 

  return 0;
}


int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  
  int ioctl_response = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  if(ioctl_response == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}



//
// --- init  ---
//
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() { 
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}

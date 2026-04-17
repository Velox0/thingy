/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)
#define BUF_SIZE 1024

/*** data ***/

struct termios orig_termios;

char *buffer;
int buf_size = BUF_SIZE;
int gap_start = 0;
int gap_end = BUF_SIZE;

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios)) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON); // fix CTRL M | disable CTRL S/Q
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP); // misc
  raw.c_oflag &= ~(OPOST); // disable POST PROCESSING
  raw.c_cflag |= (CS8); // misc
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // disable ECHO | CANONICAL MODE | CTRL V | DISABLE KEYBOARD INTERRUPT
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey(void) {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

/*** buffer operations ***/

void reallocBuffer(void) {
  int new_size = buf_size * 2;
  char *new_buffer = malloc(new_size);
  if (!new_buffer) die("malloc");

  int right_len = buf_size - gap_end;
  int new_gap_end = new_size - right_len;

  memcpy(new_buffer, buffer, gap_start);
  memcpy(new_buffer + new_gap_end, buffer + gap_end, right_len);

  free(buffer);
  buffer = new_buffer;
  buf_size = new_size;
  gap_end = new_gap_end;
}

void moveGap(int target_pos) {
  if (target_pos == gap_start) return;

  if (target_pos < gap_start) {
    int move_len = gap_start - target_pos;
    gap_start -= move_len;
    gap_end -= move_len;
    memmove(buffer + gap_end, buffer + gap_start, move_len);
  } else {
    int move_len = target_pos - gap_start;
    memmove(buffer + gap_start, buffer + gap_end, move_len);
    gap_start += move_len;
    gap_end += move_len;
  }
}


/*** output ***/

void getCursorXY(int *cx, int *cy) {
  *cx = 0;
  *cy = 0;
  for (int i = 0; i < gap_start; i++) {
    if (buffer[i] == '\n') {
      (*cy)++;
    } else if (buffer[i] == '\r') {
      *cx = 0;
    } else {
      (*cx)++;
    }
  }
}

void editorRefreshScreen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  write(STDOUT_FILENO, buffer, gap_start);
  if (buf_size - gap_end > 0) {
    write(STDOUT_FILENO, buffer + gap_end, buf_size - gap_end);
  }

  int cx, cy;
  getCursorXY(&cx, &cy);

  char buf[32];
  int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cy + 1, cx + 1);
  write(STDOUT_FILENO, buf, len);
}

/*** input ***/

void editorInsertChar(char c) {
  if (gap_start == gap_end) {
    reallocBuffer();
  }
  buffer[gap_start++] = c;
}

void editorDelChar(void) {
  if (gap_start > 0) {
    gap_start--;
  }
}

int buf_len(void) {
  return buf_size - gap_end + gap_start;
}

char charAt(int i) {
  if (i < gap_start) return buffer[i];
  return buffer[i + (gap_end - gap_start)];
}

void editorProcessKeypress(void) {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case 127: // backspace
    case 8: // ctrl-h
      if (gap_start >= 2 && buffer[gap_start - 1] == '\n' && buffer[gap_start - 2] == '\r') {
        editorDelChar(); // del \n
        editorDelChar(); // del \r
      } else {
        editorDelChar();
      }
      break;

    case '\x1b': { // escape sequence (arrows)
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1) break;
      if (read(STDIN_FILENO, &seq[1], 1) != 1) break;
      if (seq[0] == '[') {
        switch (seq[1]) {
          case 'C': // Right
            if (gap_start < buf_len()) {
              if (charAt(gap_start) == '\r' && gap_start + 1 < buf_len() && charAt(gap_start + 1) == '\n') {
                moveGap(gap_start + 2);
              } else {
                moveGap(gap_start + 1);
              }
            }
            break;
          case 'D': // Left
            if (gap_start > 0) {
              if (buffer[gap_start - 1] == '\n' && gap_start >= 2 && buffer[gap_start - 2] == '\r') {
                moveGap(gap_start - 2);
              } else {
                moveGap(gap_start - 1);
              }
            }
            break;
        }
      }
      break;
    }

    default:
      if (!iscntrl(c) || c == '\n' || c == '\r') {
        if (c == '\r' || c == '\n') {
          editorInsertChar('\r');
          editorInsertChar('\n');
        } else {
          editorInsertChar(c);
        }
      }
      break;
  }
}

/*** init ***/

int main(void) {
  buffer = malloc(BUF_SIZE);
  if (!buffer) die("malloc");

  enableRawMode();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

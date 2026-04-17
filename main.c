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

char buffer[BUF_SIZE];
int buf_len = 0;
int cursor_pos = 0;

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
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

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

/*** output ***/

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  write(STDOUT_FILENO, buffer, buf_len);

  if (cursor_pos < buf_len) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%dD", buf_len - cursor_pos);
    write(STDOUT_FILENO, buf, len);
  }
}

/*** input ***/

void editorInsertChar(char c) {
  if (buf_len < BUF_SIZE - 1) {
    memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], buf_len - cursor_pos);
    buffer[cursor_pos] = c;
    buf_len++;
    cursor_pos++;
  }
}

void editorDelChar() {
  if (cursor_pos > 0) {
    memmove(&buffer[cursor_pos - 1], &buffer[cursor_pos], buf_len - cursor_pos);
    buf_len--;
    cursor_pos--;
  }
}

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case 127: // backspace
    case 8: // ctrl-h
      editorDelChar();
      break;

    case '\x1b': { // escape sequence (arrows)
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1) break;
      if (read(STDIN_FILENO, &seq[1], 1) != 1) break;
      if (seq[0] == '[') {
        switch (seq[1]) {
          case 'C': // Right
            if (cursor_pos < buf_len) cursor_pos++;
            break;
          case 'D': // Left
            if (cursor_pos > 0) cursor_pos--;
            break;
        }
      }
      break;
    }

    default:
      if (!iscntrl(c) || c == '\n' || c == '\r') {
        if (c == '\r') c = '\n';
        editorInsertChar(c);
      }
      break;
  }
}

/*** init ***/

int main() {
  enableRawMode();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http:
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http:
 * - http:
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "linenoise.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static linenoiseCompletionCallback *completionCallback = NULL;
static linenoiseHintsCallback *hintsCallback = NULL;
static linenoiseFreeHintsCallback *freeHintsCallback = NULL;
static char *linenoiseNoTTY(void);
static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags);
static void refreshLineWithFlags(struct linenoiseState *l, int flags);

static struct termios orig_termios; 
static int maskmode = 0; 
static int rawmode = 0; 
static int mlmode = 0;  
static int atexit_registered = 0; 
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

enum KEY_ACTION{
	KEY_NULL = 0,	    
	CTRL_A = 1,         
	CTRL_B = 2,         
	CTRL_C = 3,         
	CTRL_D = 4,         
	CTRL_E = 5,         
	CTRL_F = 6,         
	CTRL_H = 8,         
	TAB = 9,            
	CTRL_K = 11,        
	CTRL_L = 12,        
	NEWLINE = 10,
	ENTER = 13,         
	CTRL_N = 14,        
	CTRL_P = 16,        
	CTRL_T = 20,        
	CTRL_U = 21,        
	CTRL_W = 23,        
	ESC = 27,           
	BACKSPACE =  127    
};

static void linenoiseAtExit(void);
int linenoiseHistoryAdd(const char *line);
#define REFRESH_CLEAN (1<<0)    
#define REFRESH_WRITE (1<<1)    
#define REFRESH_ALL (REFRESH_CLEAN|REFRESH_WRITE) 
static void refreshLine(struct linenoiseState *l);


#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)l->len,(int)l->pos,(int)l->oldpos,plen,rows,rpos, \
            (int)l->oldrows,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif




void linenoiseMaskModeEnable(void) {
    maskmode = 1;
}


void linenoiseMaskModeDisable(void) {
    maskmode = 0;
}


void linenoiseSetMultiLine(int ml) {
    mlmode = ml;
}


static int isUnsupportedTerm(void) {
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return 0;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term,unsupported_term[j])) return 1;
    return 0;
}


static int enableRawMode(int fd) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  
    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    raw.c_oflag &= ~(OPOST);
    
    raw.c_cflag |= (CS8);
    
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; 

    
    if (tcsetattr(fd,TCSADRAIN,&raw) < 0) goto fatal;
    if (write(STDOUT_FILENO, "\x1b[?200h", 7) == -1) {}
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disableRawMode(int fd) {
    if (rawmode && tcsetattr(fd,TCSADRAIN,&orig_termios) != -1) {
        if (write(STDOUT_FILENO, "\x1b[?200l", 7) == -1) {}
        rawmode = 0;
    }
}


static int getCursorPosition(int ifd, int ofd) {
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",&rows,&cols) != 2) return -1;
    return cols;
}


static int getColumns(int ifd, int ofd) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        
        int start, cols;

        
        start = getCursorPosition(ifd,ofd);
        if (start == -1) goto failed;

        
        if (write(ofd,"\x1b[999C",6) != 6) goto failed;
        cols = getCursorPosition(ifd,ofd);
        if (cols == -1) goto failed;

        
        if (cols > start) {
            char seq[32];
            snprintf(seq,32,"\x1b[%dD",cols-start);
            if (write(ofd,seq,strlen(seq)) == -1) {
                
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }

failed:
    return 80;
}


void linenoiseClearScreen(void) {
    if (write(STDOUT_FILENO,"\x1b[H\x1b[2J",7) <= 0) {
        
    }
}


static void linenoiseBeep(void) {
    fprintf(stderr, "\x7");
    fflush(stderr);
}




static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    if (lc->cvec != NULL)
        free(lc->cvec);
}


static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags) {
    
    linenoiseCompletions ctable = { 0, NULL };
    if (lc == NULL) {
        completionCallback(ls->buf,&ctable);
        lc = &ctable;
    }

    
    if (ls->completion_idx < lc->len) {
        struct linenoiseState saved = *ls;
        ls->len = ls->pos = strlen(lc->cvec[ls->completion_idx]);
        ls->buf = lc->cvec[ls->completion_idx];
        refreshLineWithFlags(ls,flags);
        ls->len = saved.len;
        ls->pos = saved.pos;
        ls->buf = saved.buf;
    } else {
        refreshLineWithFlags(ls,flags);
    }

    
    if (lc != &ctable) freeCompletions(&ctable);
}


static int completeLine(struct linenoiseState *ls, int keypressed) {
    linenoiseCompletions lc = { 0, NULL };
    int nwritten;
    char c = keypressed;

    completionCallback(ls->buf,&lc);
    if (lc.len == 0) {
        linenoiseBeep();
        ls->in_completion = 0;
    } else {
        switch(c) {
            case 9: 
                if (ls->in_completion == 0) {
                    ls->in_completion = 1;
                    ls->completion_idx = 0;
                } else {
                    ls->completion_idx = (ls->completion_idx+1) % (lc.len+1);
                    if (ls->completion_idx == lc.len) linenoiseBeep();
                }
                c = 0;
                break;
            case 27: 
                
                if (ls->completion_idx < lc.len) refreshLine(ls);
                ls->in_completion = 0;
                c = 0;
                break;
            default:
                
                if (ls->completion_idx < lc.len) {
                    nwritten = snprintf(ls->buf,ls->buflen,"%s",
                        lc.cvec[ls->completion_idx]);
                    ls->len = ls->pos = nwritten;
                }
                ls->in_completion = 0;
                break;
        }

        
        if (ls->in_completion && ls->completion_idx < lc.len) {
            refreshLineWithCompletion(ls,&lc,REFRESH_ALL);
        } else {
            refreshLine(ls);
        }
    }

    freeCompletions(&lc);
    return c; 
}


void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
    completionCallback = fn;
}


void linenoiseSetHintsCallback(linenoiseHintsCallback *fn) {
    hintsCallback = fn;
}


void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *fn) {
    freeHintsCallback = fn;
}


void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}




struct abuf {
    char *b;
    int len;
};

static void abInit(struct abuf *ab) {
    ab->b = NULL;
    ab->len = 0;
}

static void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

static void abFree(struct abuf *ab) {
    free(ab->b);
}


void refreshShowHints(struct abuf *ab, struct linenoiseState *l, int plen) {
    char seq[64];
    if (hintsCallback && plen+l->len < l->cols) {
        int color = -1, bold = 0;
        char *hint = hintsCallback(l->buf,&color,&bold);
        if (hint) {
            int hintlen = strlen(hint);
            int hintmaxlen = l->cols-(plen+l->len);
            if (hintlen > hintmaxlen) hintlen = hintmaxlen;
            if (bold == 1 && color == -1) color = 37;
            if (color != -1 || bold != 0)
                snprintf(seq,64,"\033[%d;%d;49m",bold,color);
            else
                seq[0] = '\0';
            abAppend(ab,seq,strlen(seq));
            abAppend(ab,hint,hintlen);
            if (color != -1 || bold != 0)
                abAppend(ab,"\033[0m",4);
            
            if (freeHintsCallback) freeHintsCallback(hint);
        }
    }
}


static void refreshSingleLine(struct linenoiseState *l, int flags) {
    char seq[64];
    size_t plen = strlen(l->prompt);
    int fd = l->ofd;
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;
    struct abuf ab;

    while((plen+pos) >= l->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > l->cols) {
        len--;
    }

    abInit(&ab);
    
    snprintf(seq,sizeof(seq),"\r");
    abAppend(&ab,seq,strlen(seq));

    if (flags & REFRESH_WRITE) {
        
        abAppend(&ab,l->prompt,strlen(l->prompt));
        if (maskmode == 1) {
            while (len--) abAppend(&ab,"*",1);
        } else {
            abAppend(&ab,buf,len);
        }
        
        refreshShowHints(&ab,l,plen);
    }

    
    snprintf(seq,sizeof(seq),"\x1b[0K");
    abAppend(&ab,seq,strlen(seq));

    if (flags & REFRESH_WRITE) {
        
        snprintf(seq,sizeof(seq),"\r\x1b[%dC", (int)(pos+plen));
        abAppend(&ab,seq,strlen(seq));
    }

    if (write(fd,ab.b,ab.len) == -1) {} 
    abFree(&ab);
}


static void refreshMultiLine(struct linenoiseState *l, int flags) {
    char seq[64];
    int plen = (int)l->plen;
    int col = plen;
    int rows = 1;
    int rpos2 = 1;
    int col2 = 0;
    size_t i;

    
    for (i = 0; i < l->len; i++) {
        if (l->buf[i] == '\n') {
            rows++;
            col = 0;
        } else {
            col++;
            if (col == (int)l->cols) {
                rows++;
                col = 0;
            }
        }
        if (i + 1 == l->pos) {
            rpos2 = rows;
            col2 = col;
        }
    }
    if (l->pos == 0) {
        rpos2 = 1;
        col2 = plen;
    }

    int old_rows = l->oldrows;
    int fd = l->ofd, j;
    struct abuf ab;

    l->oldrows = rows;
    abInit(&ab);

    if (flags & REFRESH_CLEAN) {
        
        int rpos_old = (plen + (int)l->oldpos + (int)l->cols) / (int)l->cols; 
        if (old_rows - rpos_old > 0) {
            snprintf(seq,64,"\x1b[%dB", old_rows - rpos_old);
            abAppend(&ab,seq,strlen(seq));
        }
        for (j = 0; j < old_rows - 1; j++) {
            snprintf(seq,64,"\r\x1b[0K\x1b[1A");
            abAppend(&ab,seq,strlen(seq));
        }
    }

    if (flags & REFRESH_ALL) {
        snprintf(seq,64,"\r\x1b[0K");
        abAppend(&ab,seq,strlen(seq));
    }

    if (flags & REFRESH_WRITE) {
        abAppend(&ab,l->prompt,plen);
        if (maskmode == 1) {
            for (i = 0; i < l->len; i++) abAppend(&ab,"*",1);
        } else {
            for (i = 0; i < l->len; i++) {
                if (l->buf[i] == '\n') {
                    abAppend(&ab,"\n",1);
                    
                    
                } else {
                    abAppend(&ab,l->buf+i,1);
                }
            }
        }

        refreshShowHints(&ab,l,plen);

        
        if (rows - rpos2 > 0) {
            snprintf(seq,64,"\x1b[%dA", rows - rpos2);
            abAppend(&ab,seq,strlen(seq));
        }

        
        if (col2)
            snprintf(seq,64,"\r\x1b[%dC", col2);
        else
            snprintf(seq,64,"\r");
        abAppend(&ab,seq,strlen(seq));
    }

    l->oldpos = l->pos;
    if (write(fd,ab.b,ab.len) == -1) {}
    abFree(&ab);
}


static void refreshLineWithFlags(struct linenoiseState *l, int flags) {
    if (mlmode)
        refreshMultiLine(l,flags);
    else
        refreshSingleLine(l,flags);
}


static void refreshLine(struct linenoiseState *l) {
    refreshLineWithFlags(l,REFRESH_ALL);
}


void linenoiseHide(struct linenoiseState *l) {
    if (mlmode)
        refreshMultiLine(l,REFRESH_CLEAN);
    else
        refreshSingleLine(l,REFRESH_CLEAN);
}


void linenoiseShow(struct linenoiseState *l) {
    if (l->in_completion) {
        refreshLineWithCompletion(l,NULL,REFRESH_WRITE);
    } else {
        refreshLineWithFlags(l,REFRESH_WRITE);
    }
}


int linenoiseEditInsert(struct linenoiseState *l, char c) {
    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            l->buf[l->pos] = c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
            if ((!mlmode && l->plen+l->len < l->cols && !hintsCallback)) {
                
                char d = (maskmode==1) ? '*' : c;
                if (write(l->ofd,&d,1) == -1) return -1;
            } else {
                refreshLine(l);
            }
        } else {
            memmove(l->buf+l->pos+1,l->buf+l->pos,l->len-l->pos);
            l->buf[l->pos] = c;
            l->len++;
            l->pos++;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    }
    return 0;
}


void linenoiseEditMoveLeft(struct linenoiseState *l) {
    if (l->pos > 0) {
        l->pos--;
        refreshLine(l);
    }
}


void linenoiseEditMoveRight(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos++;
        refreshLine(l);
    }
}


void linenoiseEditMoveHome(struct linenoiseState *l) {
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}


void linenoiseEditMoveEnd(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}


#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
void linenoiseEditHistoryNext(struct linenoiseState *l, int dir) {
    if (history_len > 1) {
        
        free(history[history_len - 1 - l->history_index]);
        history[history_len - 1 - l->history_index] = strdup(l->buf);
        
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= history_len) {
            l->history_index = history_len-1;
            return;
        }
        strncpy(l->buf,history[history_len - 1 - l->history_index],l->buflen);
        l->buf[l->buflen-1] = '\0';
        l->len = l->pos = strlen(l->buf);
        refreshLine(l);
    }
}


void linenoiseEditDelete(struct linenoiseState *l) {
    if (l->len > 0 && l->pos < l->len) {
        memmove(l->buf+l->pos,l->buf+l->pos+1,l->len-l->pos-1);
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}


void linenoiseEditBackspace(struct linenoiseState *l) {
    if (l->pos > 0 && l->len > 0) {
        memmove(l->buf+l->pos-1,l->buf+l->pos,l->len-l->pos);
        l->pos--;
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}


void linenoiseEditDeletePrevWord(struct linenoiseState *l) {
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos--;
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos,l->buf+old_pos,l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}


int linenoiseEditStart(struct linenoiseState *l, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt) {
    
    l->in_completion = 0;
    l->ifd = stdin_fd != -1 ? stdin_fd : STDIN_FILENO;
    l->ofd = stdout_fd != -1 ? stdout_fd : STDOUT_FILENO;
    l->buf = buf;
    l->buflen = buflen;
    l->prompt = prompt;
    l->plen = strlen(prompt);
    l->oldpos = l->pos = 0;
    l->len = 0;

    
    if (enableRawMode(l->ifd) == -1) return -1;

    l->cols = getColumns(stdin_fd, stdout_fd);
    l->oldrows = 0;
    l->history_index = 0;

    
    l->buf[0] = '\0';
    l->buflen--; 

    
    if (!isatty(l->ifd)) return 0;

    
    linenoiseHistoryAdd("");

    if (write(l->ofd,prompt,l->plen) == -1) return -1;
    return 0;
}

char *linenoiseEditMore = "If you see this, you are misusing the API: when linenoiseEditFeed() is called, if it returns linenoiseEditMore the user is yet editing the line. See the README file for more information.";


char *linenoiseEditFeed(struct linenoiseState *l) {
    
    if (!isatty(l->ifd)) return linenoiseNoTTY();

    char c;
    int nread;
    char seq[8];

    nread = read(l->ifd,&c,1);
    if (nread <= 0) return NULL;

    
    if ((l->in_completion || c == 9) && completionCallback != NULL) {
        c = completeLine(l,c);
        
        if (c < 0) return NULL;
        
        if (c == 0) return linenoiseEditMore;
    }

    switch(c) {
    case NEWLINE:
    case ENTER:    
        history_len--;
        free(history[history_len]);
        if (mlmode) linenoiseEditMoveEnd(l);
        if (hintsCallback) {
            
            linenoiseHintsCallback *hc = hintsCallback;
            hintsCallback = NULL;
            refreshLine(l);
            hintsCallback = hc;
        }
        return strdup(l->buf);
    case CTRL_C:     
        errno = EAGAIN;
        return NULL;
    case BACKSPACE:   
    case 8:     
        linenoiseEditBackspace(l);
        break;
    case CTRL_D:     
        if (l->len > 0) {
            linenoiseEditDelete(l);
        } else {
            history_len--;
            free(history[history_len]);
            errno = ENOENT;
            return NULL;
        }
        break;
    case CTRL_T:    
        if (l->pos > 0 && l->pos < l->len) {
            int aux = l->buf[l->pos-1];
            l->buf[l->pos-1] = l->buf[l->pos];
            l->buf[l->pos] = aux;
            if (l->pos != l->len-1) l->pos++;
            refreshLine(l);
        }
        break;
    case CTRL_B:     
        linenoiseEditMoveLeft(l);
        break;
    case CTRL_F:     
        linenoiseEditMoveRight(l);
        break;
    case CTRL_P:    
        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
        break;
    case CTRL_N:    
        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
        break;
    case ESC:    
        
        if (read(l->ifd,seq,1) == -1) break;
        if (read(l->ifd,seq+1,1) == -1) break;

        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                
                if (read(l->ifd,seq+2,1) == -1) break;
                if (seq[2] == '~') {
                    switch(seq[1]) {
                    case '3': 
                        linenoiseEditDelete(l);
                        break;
                    }
                } else if (seq[1] == '2' && seq[2] == '0' && read(l->ifd,seq+3,2) == 2 && seq[3] == '0' && seq[4] == '~') {
                    while (1) {
                        if (read(l->ifd,&c,1) <= 0) break;
                        if (c == '\x1b') {
                            if (read(l->ifd,seq,5) == 5 && !memcmp(seq,"[201~",5)) break;
                            continue;
                        }
                        linenoiseEditInsert(l,c);
                    }
                    if (l->len > 0 && (l->buf[l->len-1] == '\n' || l->buf[l->len-1] == '\r')) {
                        history_len--;
                        free(history[history_len]);
                        if (mlmode) linenoiseEditMoveEnd(l);
                        if (hintsCallback) {
                            linenoiseHintsCallback *hc = hintsCallback;
                            hintsCallback = NULL;
                            refreshLine(l);
                            hintsCallback = hc;
                        }
                        return strdup(l->buf);
                    }
                }
            } else {
                switch(seq[1]) {
                case 'A': 
                    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
                    break;
                case 'B': 
                    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
                    break;
                case 'C': 
                    linenoiseEditMoveRight(l);
                    break;
                case 'D': 
                    linenoiseEditMoveLeft(l);
                    break;
                case 'H': 
                    linenoiseEditMoveHome(l);
                    break;
                case 'F': 
                    linenoiseEditMoveEnd(l);
                    break;
                }
            }
        }

        
        else if (seq[0] == 'O') {
            switch(seq[1]) {
            case 'H': 
                linenoiseEditMoveHome(l);
                break;
            case 'F': 
                linenoiseEditMoveEnd(l);
                break;
            }
        }
        break;
    default:
        if (linenoiseEditInsert(l,c)) return NULL;
        break;
    case CTRL_U: 
        l->buf[0] = '\0';
        l->pos = l->len = 0;
        refreshLine(l);
        break;
    case CTRL_K: 
        l->buf[l->pos] = '\0';
        l->len = l->pos;
        refreshLine(l);
        break;
    case CTRL_A: 
        linenoiseEditMoveHome(l);
        break;
    case CTRL_E: 
        linenoiseEditMoveEnd(l);
        break;
    case CTRL_L: 
        linenoiseClearScreen();
        refreshLine(l);
        break;
    case CTRL_W: 
        linenoiseEditDeletePrevWord(l);
        break;
    }
    return linenoiseEditMore;
}


void linenoiseEditStop(struct linenoiseState *l) {
    if (!isatty(l->ifd)) return;
    disableRawMode(l->ifd);
    printf("\n");
}


static char *linenoiseBlockingEdit(int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt)
{
    struct linenoiseState l;

    
    if (buflen == 0) {
        errno = EINVAL;
        return NULL;
    }

    linenoiseEditStart(&l,stdin_fd,stdout_fd,buf,buflen,prompt);
    char *res;
    while((res = linenoiseEditFeed(&l)) == linenoiseEditMore);
    linenoiseEditStop(&l);
    return res;
}


void linenoisePrintKeyCodes(void) {
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
            "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (enableRawMode(STDIN_FILENO) == -1) return;
    memset(quit,' ',4);
    while(1) {
        char c;
        int nread;

        nread = read(STDIN_FILENO,&c,1);
        if (nread <= 0) continue;
        memmove(quit,quit+1,sizeof(quit)-1); 
        quit[sizeof(quit)-1] = c; 
        if (memcmp(quit,"quit",sizeof(quit)) == 0) break;

        printf("'%c' %02x (%d) (type quit to exit)\n",
            isprint(c) ? c : '?', (int)c, (int)c);
        printf("\r"); 
        fflush(stdout);
    }
    disableRawMode(STDIN_FILENO);
}


static char *linenoiseNoTTY(void) {
    char *line = NULL;
    size_t len = 0, maxlen = 0;

    while(1) {
        if (len == maxlen) {
            if (maxlen == 0) maxlen = 16;
            maxlen *= 2;
            char *oldval = line;
            line = realloc(line,maxlen);
            if (line == NULL) {
                if (oldval) free(oldval);
                return NULL;
            }
        }
        int c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            if (c == EOF && len == 0) {
                free(line);
                return NULL;
            } else {
                line[len] = '\0';
                return line;
            }
        } else {
            line[len] = c;
            len++;
        }
    }
}


char *linenoise(const char *prompt) {
    char buf[LINENOISE_MAX_LINE];

    if (!isatty(STDIN_FILENO)) {
        
        return linenoiseNoTTY();
    } else if (isUnsupportedTerm()) {
        size_t len;

        printf("%s",prompt);
        fflush(stdout);
        if (fgets(buf,LINENOISE_MAX_LINE,stdin) == NULL) return NULL;
        len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return strdup(buf);
    } else {
        char *retval = linenoiseBlockingEdit(STDIN_FILENO,STDOUT_FILENO,buf,LINENOISE_MAX_LINE,prompt);
        return retval;
    }
}


void linenoiseFree(void *ptr) {
    if (ptr == linenoiseEditMore) return; 
    free(ptr);
}




static void freeHistory(void) {
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
    }
}


static void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
    freeHistory();
}


int linenoiseHistoryAdd(const char *line) {
    char *linecopy;

    if (history_max_len == 0) return 0;

    
    if (history == NULL) {
        history = malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }

    
    if (history_len && !strcmp(history[history_len-1], line)) return 0;

    
    linecopy = strdup(line);
    if (!linecopy) return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history,history+1,sizeof(char*)*(history_max_len-1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
}


int linenoiseHistorySetMaxLen(int len) {
    char **new;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        new = malloc(sizeof(char*)*len);
        if (new == NULL) return 0;

        
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++) free(history[j]);
            tocopy = len;
        }
        memset(new,0,sizeof(char*)*len);
        memcpy(new,history+(history_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = new;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}


int linenoiseHistorySave(const char *filename) {
    mode_t old_umask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
    FILE *fp;
    int j;

    fp = fopen(filename,"w");
    umask(old_umask);
    if (fp == NULL) return -1;
    chmod(filename,S_IRUSR|S_IWUSR);
    for (j = 0; j < history_len; j++)
        fprintf(fp,"%s\n",history[j]);
    fclose(fp);
    return 0;
}


int linenoiseHistoryLoad(const char *filename) {
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}

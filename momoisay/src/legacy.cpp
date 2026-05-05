#include <getopt.h>
#include <ncurses.h>
#include <sys/types.h>
#include <unistd.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "art.hpp"
#include "momoisay.hpp"

#define STATIC_V1_X 15
#define STATIC_V1_Y 110
#define STATIC_V1_RY 38
#define ANIMATED_V1_X 30
#define ANIMATED_V1_Y 187
#define ANIMATED_V1_RY 62
#define ANIMATED_V2_X 30
#define ANIMATED_V2_Y 189
#define ANIMATED_V2_RY 62
#define ANIMATED_V3_X 30
#define ANIMATED_V3_Y 189
#define ANIMATED_V3_RY 62
#define ANIMATED_MY 189
#define STATIC_VERSION 1
#define ANIMATED_VERSION 3
#define MAX_LENGTH 30

/**
 * Initialize ncurses and configure the terminal for interactive UI usage.
 *
 * This function sets up the terminal into a controlled, non-canonical mode
 * suitable for real-time input handling and full-screen rendering.
 *
 * Effects (in order):
 *
 * 1. setlocale(LC_ALL, "")
 *    Enables the system locale so ncurses can correctly handle multibyte
 *    characters (UTF-8, wide chars, etc.). Without this, drawing non-ASCII
 *    may break or render incorrectly.
 *
 * 2. initscr()
 *    Initializes the ncurses library and switches the terminal into
 *    "curses mode". This:
 *      - Allocates internal screen structures
 *      - Detects terminal capabilities
 *      - Switches to an alternate screen buffer
 *
 *    Must be called before almost all other ncurses functions.
 *
 * 3. cbreak()
 *    Disables line buffering. Normally, terminal input is buffered until
 *    newline (canonical mode). In cbreak mode, each keypress becomes
 *    immediately available to the program.
 * :contentReference[oaicite:0]{index=0}
 *
 *    This is required for responsive input (games, TUIs, etc.).
 *
 * 4. noecho()
 *    Prevents typed characters from being automatically printed to the screen.
 *    By default, input is echoed; disabling it allows full control over
 * rendering. :contentReference[oaicite:1]{index=1}
 *
 *    Without this, user keypresses would appear on screen unpredictably.
 *
 * 5. keypad(stdscr, TRUE)
 *    Enables special key handling (arrow keys, function keys, etc.).
 *    Instead of receiving raw escape sequences, getch() returns symbolic
 *    constants like KEY_UP, KEY_LEFT, etc.
 *
 *    Applies to the main window (stdscr).
 *
 * 6. curs_set(0)
 *    Hides the terminal cursor.
 *    Useful for rendering UIs where the cursor is distracting or meaningless.
 *
 * 7. timeout(-1)
 *    Configures blocking input mode:
 *      - getch() will block indefinitely until a key is pressed.
 *
 *    Behavior:
 *      delay < 0  → blocking (wait forever)
 *      delay = 0  → non-blocking (poll)
 *      delay > 0  → timed wait (milliseconds)
 *
 *    This ensures the program pauses for input instead of busy-looping.
 * :contentReference[oaicite:2]{index=2}
 *
 * Summary:
 *   After this function:
 *     - Input is immediate (no line buffering)
 *     - Input is not echoed
 *     - Special keys are decoded
 *     - Cursor is hidden
 *     - Input calls block until keypress
 *
 * Notes:
 *   - There is no error handling; all ncurses calls can fail.
 *   - No matching teardown here (endwin() must be called elsewhere).
 *   - Mixing raw()/cbreak() elsewhere can lead to undefined terminal states.
 */
static void init() {
    setlocale( LC_ALL, "" );

    initscr();

    cbreak();

    noecho();

    keypad( stdscr, TRUE );

    curs_set( 0 );

    timeout( -1 );
}

#if 0
"    <text>                              Text that cute Momoi will "
"say!!! (default static version 1)\n" );
#endif

/**
 * Count how many canvas rows are needed to print argv[_start.._end)
 * with word-wrapping at MAX_LENGTH columns.
 *
 * The logic treats each argument as text and counts one extra character
 * between arguments, as if they were separated by spaces.
 *
 * Returns:
 *   Number of rows needed to render the text.
 */
static auto getLine( char** _argv, int _start, int _end ) -> int {
    int l_lines = 0;
    int l_cnt = 0;

    /* If there is at least one argument, the output occupies at least one line.
     */
    if ( _end - _start ) {
        l_lines++;
    }

    for ( int l_i = _start; l_i < _end; l_i++ ) {
        char* l_str = _argv[ l_i ];

        while ( *l_str != '\0' ) {
            /* Wrap to a new line once the current line reaches MAX_LENGTH. */
            if ( l_cnt >= MAX_LENGTH ) {
                l_cnt = 0;
                l_lines++;
            }

            l_cnt++;
            l_str++;
        }

        /* Count the separator between words/arguments. */
        l_cnt++;
    }

    return ( l_lines );
}

/**
 * Allocate a 2D character buffer representing a text canvas.
 *
 * _x = number of rows
 * _y = number of columns per row
 *
 * Each row is allocated as a zero-initialized C string of length _y + 1,
 * so it can be used with "%s" safely.
 *
 * Returns:
 *   Pointer to an array of row pointers, or nullptr/invalid allocation result
 *   depending on malloc/calloc behavior.
 */
static auto createCanvas( int _x, int _y ) -> char** {
    char** l_canvas = ( char** )malloc( _x * sizeof( char* ) );

    for ( int l_i = 0; l_i < _x; l_i++ ) {
        l_canvas[ l_i ] = ( char* )calloc( _y + 1, sizeof( char ) );
    }

    return ( l_canvas );
}

/**
 * Print each row of the canvas at screen position (_px, _py) using ncurses.
 *
 * Each canvas row is printed on the next terminal line.
 * refresh() is called after all rows are drawn.
 */
static void printCanvas( char** _canvas, int _x, int _px, int _py ) {
    for ( int l_i = 0; l_i < _x; l_i++ ) {
        mvprintw( _py + l_i, _px, "%s", _canvas[ l_i ] );
    }

    refresh();
}

/**
 * Free a canvas created by createCanvas().
 *
 * Safe to call with nullptr.
 */
static void freeCanvas( char** _canvas, int _x ) {
    if ( _canvas == nullptr ) {
        return;
    }

    for ( int l_i = 0; l_i < _x; l_i++ ) {
        free( _canvas[ l_i ] );
    }

    free( _canvas );
}

/**
 * Compute the total text length of argv[_start.._end), including one space
 * between arguments, then clamp the result to MAX_LENGTH.
 *
 * Important:
 *   As written, this returns -1 when _start == _end because of:
 *     return ( l_length - 1 );
 *   If empty input is possible, that is probably a bug.
 *
 * Returns:
 *   Total printable length, clamped to MAX_LENGTH.
 */
static auto textlen( char* _argv[], int _start, int _end ) -> int {
    int l_length = 0;

    for ( int l_i = _start; l_i < _end; l_i++ ) {
        l_length += strlen( _argv[ l_i ] ) + 1;
    }

    if ( l_length - 1 > MAX_LENGTH ) {
        return ( MAX_LENGTH );
    }

    return ( l_length - 1 );
}

static void constructV1( std::span< const art::frame_t > _art,
                         char* _argv[],
                         std::span< const int > _intervals,
                         int _frames,
                         int _x,
                         int _y,
                         int _ry,
                         int _length,
                         int _lines,
                         int _start,
                         int _end,
                         int _round ) {
    int l_currentFrame = 0;

    while ( _round != 0 ) {
        nodelay( stdscr, TRUE );

        int l_ch = getch();

        if ( l_ch == 113 || l_ch == 81 ) {
            endwin();
            exit( 0 );
        }

        int l_cnt = 0, l_pt1 = ( _x + 1 ) / 2, l_pt2 = ( ( _x + 1 ) / 2 ) + 1,
            l_pts = 3 + _y, l_ptt = l_pts;

        char** l_canvas = createCanvas( _x, _y + _length );

        erase();

        int l_terminalHeight = LINES;
        int l_terminalWidth = COLS;
        int l_px = ( l_terminalWidth - _ry - _length ) / 2;
        int l_py = ( l_terminalHeight - _x ) / 2;

        for ( int l_i = 0; l_i < _x; l_i++ ) {
            int l_len = _art[ l_currentFrame ][ l_i ].size();

            for ( int l_j = 0; l_j < _y + _length; l_j++ ) {
                if ( l_j < l_len ) {
                    l_canvas[ l_i ][ l_j ] =
                        _art[ l_currentFrame ][ l_i ][ l_j ];

                } else if ( l_canvas[ l_i ][ l_j ] == '\0' ) {
                    l_canvas[ l_i ][ l_j ] = ' ';
                }

                if ( !l_i && _length ) {
                    if ( l_j == _y ) {
                        l_canvas[ l_pt1-- ][ l_j ] = '/';
                        l_canvas[ l_pt2++ ][ l_j ] = '\\';

                    } else if ( l_j - 1 == _y ) {
                        for ( int l_cnt = 0; l_cnt < _lines / 2; l_cnt++ ) {
                            l_canvas[ l_pt1-- ][ l_j ] = '|';
                            l_canvas[ l_pt2++ ][ l_j ] = '|';
                        }

                        l_pt2--;

                    } else if ( l_j + 1 == _y + _length ) {
                        for ( int l_k = ++l_pt1; l_k <= l_pt2; l_k++ ) {
                            l_canvas[ l_k ][ l_j ] = '|';
                        }

                        l_pt1++;

                    } else {
                        l_canvas[ l_pt1 ][ l_j ] = '_';
                        l_canvas[ l_pt2 ][ l_j ] = '_';
                    }
                }
            }

            if ( !l_cnt && ( _length || _lines ) ) {
                for ( int l_j = _start; l_j < _end; l_j++ ) {
                    char* l_str = _argv[ l_j ];

                    while ( *l_str != '\0' ) {
                        if ( l_cnt >= MAX_LENGTH ) {
                            l_pt1++;
                            l_pts = l_ptt;
                            l_cnt = 0;
                        }

                        l_canvas[ l_pt1 ][ l_pts++ ] = *l_str;
                        l_cnt++;
                        l_str++;
                    }

                    if ( l_cnt > MAX_LENGTH ) {
                        continue;
                    }

                    l_canvas[ l_pt1 ][ l_pts++ ] = ' ';
                    l_cnt++;
                }
            }

            l_cnt = 1;
        }

        printCanvas( l_canvas, _x, l_px, l_py );

        usleep( _intervals[ l_currentFrame++ ] );

        freeCanvas( l_canvas, _x );

        if ( l_currentFrame == _frames ) {
            l_currentFrame = 0;

            if ( _round > 0 ) {
                _round--;
            }
        }
    }
}

namespace {

namespace detail {

using mode_t = enum class mode : uint8_t {
    mStatic,
    animated,
};

}

detail::mode_t g_mode = detail::mode_t::mStatic;

} // namespace

namespace momoisay {

auto setStatic( [[maybe_unused]] int _key,
                [[maybe_unused]] std::string_view _value,
                [[maybe_unused]] arhodigp::state_t _state ) -> bool {
    g_mode = detail::mode_t::mStatic;

    return ( true );
}

auto setAnimated( [[maybe_unused]] int _key,
                  [[maybe_unused]] std::string_view _value,
                  [[maybe_unused]] arhodigp::state_t _state ) -> bool {
    g_mode = detail::mode::animated;

    return ( true );
}

void oneshot() {
    init();

    size_t l_length =
        5 + textlen( _argumentVector, optind + l_argctl, _argumentCount );
    size_t l_lines =
        getLine( _argumentVector, optind + l_argctl, _argumentCount );

    if ( l_length <= 5 ) {
        l_length = 0;
    }

    if ( g_mode == detail::mode_t::mStatic ) {
        if ( l_lines <= 30 ) {
            if ( l_lines & 1 ) {
                l_lines++;
            }

            constexpr std::array l_frame = { 150000, 75000, 150000, 150000,
                                             75000 };

            constructV1( art::g_momoiAnimatedV1, _argumentVector, l_frame, 5,
                         ANIMATED_V1_X, ANIMATED_V1_Y, ANIMATED_V1_RY, l_length,
                         l_lines, optind + l_argctl, _argumentCount, -1 );
        }

    } else if ( g_mode == detail::mode_t::animated ) {
        if ( l_lines <= 10 ) {
            if ( l_lines & 1 ) {
                l_lines++;
            }

            constexpr std::array l_frame = { 75000 };

            constructV1( art::g_momoiStaticV1, _argumentVector, l_frame, 1,
                         STATIC_V1_X, STATIC_V1_Y, STATIC_V1_RY, l_length,
                         l_lines, optind + l_argctl, _argumentCount, -1 );
        }
    }
}

} // namespace momoisay

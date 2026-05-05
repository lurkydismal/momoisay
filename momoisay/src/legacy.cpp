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

namespace {

#define STATIC_V1_X 15
#define STATIC_V1_Y 110
#define STATIC_V1_RY 38
#define ANIMATED_V1_X 30
#define ANIMATED_V1_Y 187
#define ANIMATED_V1_RY 62
#define ANIMATED_MY 189
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
void init() {
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
auto getLine( std::string_view _text ) -> size_t {
    if ( _text.empty() ) {
        return 0;
    }

    // If there is at least one argument, the output occupies at least one line.
    int l_lines = 1;
    int l_cnt = 0;

    for ( char l_ch : _text ) {
        if ( l_ch == '\n' ) {
            l_lines++;
            l_cnt = 0;
            continue;
        }

        /* Wrap to a new line once the current line reaches MAX_LENGTH. */
        if ( l_cnt >= MAX_LENGTH ) {
            l_lines++;
            l_cnt = 0;
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
auto createCanvas( int _x, int _y ) -> char** {
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
void printCanvas( char** _canvas, int _x, int _px, int _py ) {
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
void freeCanvas( char** _canvas, int _x ) {
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
auto textlen( std::string_view _text ) -> size_t {
    if ( _text.empty() ) {
        return ( 0 );
    }

    if ( _text.size() > MAX_LENGTH ) {
        return MAX_LENGTH;
    }

    return ( _text.size() );
}

/**
 * Build and render one animated/static "version 1" frame set inside ncurses.
 *
 * What this function does:
 *   - Polls for a quit key.
 *   - Allocates a 2D text canvas.
 *   - Copies one animation frame into the canvas.
 *   - Optionally draws an ASCII "speech bubble" / connector shape.
 *   - Optionally writes text from argv onto the canvas.
 *   - Prints the canvas to the terminal.
 *   - Sleeps for the frame-specific delay.
 *   - Frees the canvas.
 *   - Advances the animation frame and round counter.
 *
 * Arguments:
 *   _art       - All animation frames; each frame is a 2D character grid.
 *   _argv      - Text fragments to render into the canvas.
 *   _intervals - Per-frame delays in microseconds.
 *   _frames    - Total number of frames in _art / _intervals.
 *   _x         - Canvas height in rows.
 *   _y         - Base art width.
 *   _ry        - Rendering width used for horizontal centering.
 *   _length    - Extra horizontal space used for text / connector area.
 *   _lines     - Number of text lines to draw in the connector area.
 *   _start     - First argv index to render.
 *   _end       - One-past-last argv index to render.
 *   _round     - Number of animation loops remaining.
 *
 * Important behavior:
 *   - Pressing 'q' or 'Q' exits immediately.
 *   - The function blocks until _round reaches 0.
 *   - If _round is negative, the loop never naturally terminates.
 */
void constructV1( std::span< const art::frame_t > _art,
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
    /* Index of the current animation frame. */
    int l_currentFrame = 0;

    /* Keep rendering until the requested number of rounds is finished. */
    while ( _round != 0 ) {
        /* Disable input blocking so getch() returns immediately. */
        nodelay( stdscr, TRUE );

        /* Read one key without waiting. */
        int l_ch = getch();

        /* Quit immediately on 'q' or 'Q'. */
        if ( l_ch == 113 || l_ch == 81 ) {
            endwin();
            exit( 0 );
        }

        /* Initialize per-frame drawing state. */
        int l_cnt = 0,              /* Character counter for text wrapping. */
            l_pt1 = ( _x + 1 ) / 2, /* Top/left-ish connector cursor. */
            l_pt2 =
                ( ( _x + 1 ) / 2 ) + 1, /* Bottom/right-ish connector cursor. */
            l_pts = 3 + _y, /* Text start column inside the canvas. */
            l_ptt = l_pts;  /* Saved initial text start column. */

        /* Allocate a blank canvas large enough for art plus extra text width.
         */
        char** l_canvas = createCanvas( _x, _y + _length );

        /* Clear the screen before drawing the next frame. */
        erase();

        /* Read terminal dimensions from ncurses. */
        int l_terminalHeight = LINES;
        int l_terminalWidth = COLS;

        /* Center the output horizontally and vertically. */
        int l_px = ( l_terminalWidth - _ry - _length ) / 2;
        int l_py = ( l_terminalHeight - _x ) / 2;

        /* Copy the current animation frame into the canvas row by row. */
        for ( int l_i = 0; l_i < _x; l_i++ ) {
            /* Length of the current art row. */
            int l_len = _art[ l_currentFrame ][ l_i ].size();

            /* Walk across the full row width, including extra text space. */
            for ( int l_j = 0; l_j < _y + _length; l_j++ ) {
                /* If the current position is still inside the source art row,
                   copy the character into the canvas. */
                if ( l_j < l_len ) {
                    l_canvas[ l_i ][ l_j ] =
                        _art[ l_currentFrame ][ l_i ][ l_j ];

                    /* Otherwise, if the slot is still empty, fill it with a
                       space so the row behaves like a printable string. */
                } else if ( l_canvas[ l_i ][ l_j ] == '\0' ) {
                    l_canvas[ l_i ][ l_j ] = ' ';
                }

                /* Special case: only on the first art row, and only if the
                   extra text/connector area exists, draw a decorative shape
                   that visually connects the art to the text area. */
                if ( !l_i && _length ) {
                    /* At the left edge of the text area, place the opening
                       slash and the mirrored backslash on the lower row. */
                    if ( l_j == _y ) {
                        l_canvas[ l_pt1-- ][ l_j ] = '/';
                        l_canvas[ l_pt2++ ][ l_j ] = '\\';

                        /* On the row just before the opening edge, draw
                           vertical bars descending from the two connector
                           points. */
                    } else if ( l_j - 1 == _y ) {
                        for ( int l_cnt = 0; l_cnt < _lines / 2; l_cnt++ ) {
                            l_canvas[ l_pt1-- ][ l_j ] = '|';
                            l_canvas[ l_pt2++ ][ l_j ] = '|';
                        }

                        /* Undo the last increment so later drawing stays
                         * aligned. */
                        l_pt2--;

                        /* At the far edge of the text area, close the connector
                           with vertical bars spanning the middle section. */
                    } else if ( l_j + 1 == _y + _length ) {
                        for ( int l_k = ++l_pt1; l_k <= l_pt2; l_k++ ) {
                            l_canvas[ l_k ][ l_j ] = '|';
                        }

                        /* Restore l_pt1 so the next row starts from the
                         * intended spot. */
                        l_pt1++;

                        /* Everywhere else in the connector area, draw the top
                           and bottom outline using underscores. */
                    } else {
                        l_canvas[ l_pt1 ][ l_j ] = '_';
                        l_canvas[ l_pt2 ][ l_j ] = '_';
                    }
                }
            }

            /* Only write argv text once, on the first canvas row that reaches
               the text-rendering section. */
            if ( !l_cnt && ( _length || _lines ) ) {
                /* Copy argv[_start.._end) into the canvas, wrapping at
                 * MAX_LENGTH. */
                for ( int l_j = _start; l_j < _end; l_j++ ) {
                    char* l_str = _argv[ l_j ];

                    /* Copy one argument character by character. */
                    while ( *l_str != '\0' ) {
                        /* Wrap to the next output line when the current line
                           reaches the maximum allowed text width. */
                        if ( l_cnt >= MAX_LENGTH ) {
                            l_pt1++;
                            l_pts = l_ptt;
                            l_cnt = 0;
                        }

                        /* Write the current character into the canvas. */
                        l_canvas[ l_pt1 ][ l_pts++ ] = *l_str;
                        l_cnt++;
                        l_str++;
                    }

                    /* If the current line already overflowed, do not append a
                       separator space here; the next wrapped line continues. */
                    if ( l_cnt > MAX_LENGTH ) {
                        continue;
                    }

                    /* Add one separator space between arguments. */
                    l_canvas[ l_pt1 ][ l_pts++ ] = ' ';
                    l_cnt++;
                }
            }

            /* Mark that at least one row has been processed.
               This prevents the text block from being written repeatedly. */
            l_cnt = 1;
        }

        /* Draw the fully prepared canvas at the computed terminal position. */
        printCanvas( l_canvas, _x, l_px, l_py );

        /* Wait for the frame-specific delay before advancing. */
        usleep( _intervals[ l_currentFrame++ ] );

        /* Release all canvas memory for this frame. */
        freeCanvas( l_canvas, _x );

        /* Loop animation frames back to the beginning. */
        if ( l_currentFrame == _frames ) {
            l_currentFrame = 0;

            /* Count one completed animation round only if rounds are positive.
             */
            if ( _round > 0 ) {
                _round--;
            }
        }
    }
}

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

void oneshot( std::string_view _text ) {
    init();

    size_t l_length = 5 + textlen( _text );
    size_t l_lines = getLine( _text );

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

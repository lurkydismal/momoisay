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
    /* Allocate pointer table + contiguous character buffer. */
    size_t l_ptrSize = _x * sizeof( char* );
    size_t l_dataSize = ( size_t )_x * ( _y + 1 ) * sizeof( char );

    auto l_canvas =
        static_cast< gsl::owner< char** > >( malloc( l_ptrSize + l_dataSize ) );

    if ( l_canvas == nullptr ) {
        return nullptr;
    }

    /* Data block starts right after the pointer table. */
    char* l_data = ( char* )( ( char* )l_canvas + l_ptrSize );

    /* Assign row pointers into the contiguous block. */
    for ( int l_i = 0; l_i < _x; l_i++ ) {
        l_canvas[ l_i ] = l_data + ( size_t )l_i * ( _y + 1 );
    }

    /* Zero the entire character buffer (like calloc). */
    memset( l_data, 0, l_dataSize );

    return l_canvas;
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
void freeCanvas( char** _canvas ) {
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
 * Render one animated "version 1" scene in ncurses.
 *
 * This function:
 *   - checks for a quit key,
 *   - allocates a temporary character canvas,
 *   - copies the current art frame into that canvas,
 *   - draws the connector / frame outline on the right side,
 *   - writes a single text block from std::string_view into the extra area,
 *   - prints the completed canvas,
 *   - waits for the current frame delay,
 *   - frees the canvas,
 *   - advances the animation frame,
 *   - repeats until _round reaches zero.
 *
 * Parameters:
 *   _art       - Animation frames. Each frame is a 2D character grid.
 *   _text      - Text to render into the connector / speech area.
 *   _intervals - Per-frame delays in microseconds.
 *   _frames    - Number of frames in _art and _intervals.
 *   _x         - Canvas height, in rows.
 *   _y         - Base art width.
 *   _ry        - Width used for centering the rendered content.
 *   _length    - Extra horizontal space reserved for text / connector drawing.
 *   _lines     - Number of connector text lines to draw.
 *   _round     - Number of animation loops remaining.
 *
 * Notes:
 *   - Pressing q or Q exits immediately.
 *   - _text is non-owning; the referenced storage must remain valid.
 *   - This function assumes all spans and indices are valid.
 */
void constructV1( std::span< const art::frame_t > _art,
                  std::string_view _text,
                  std::span< const int > _intervals,
                  int _frames,
                  int _x,
                  int _y,
                  int _ry,
                  int _length,
                  int _lines,
                  int _round ) {
    /* Current animation frame index. */
    int l_currentFrame = 0;

    /* Keep rendering until the requested number of rounds is exhausted. */
    while ( _round != 0 ) {
        /* Make getch() non-blocking for this iteration. */
        nodelay( stdscr, TRUE );

        /* Read one key if available. With nodelay enabled, this returns
           immediately instead of waiting for input. */
        int l_ch = getch();

        /* Exit immediately on q or Q. */
        if ( l_ch == 'q' || l_ch == 'Q' ) {
            endwin();
            exit( 0 );
        }

        /* Per-frame drawing state:
           l_cnt - general character counter used for wrapping text
           l_pt1 - upper connector cursor / top text row anchor
           l_pt2 - lower connector cursor / bottom text row anchor
           l_pts - current text column in the canvas
           l_ptt - saved initial text column for wrapping reset */
        int l_cnt = 0, l_pt1 = ( _x + 1 ) / 2, l_pt2 = ( ( _x + 1 ) / 2 ) + 1,
            l_pts = 3 + _y, l_ptt = l_pts;

        /* Allocate a blank canvas large enough for the art and the extra text
         * area. */
        char** l_canvas = createCanvas( _x, _y + _length );

        /* Clear the screen before drawing the new frame. */
        erase();

        /* Get current terminal dimensions from ncurses. */
        int l_terminalHeight = LINES;
        int l_terminalWidth = COLS;

        /* Compute the top-left draw position so the content is centered. */
        int l_px = ( l_terminalWidth - _ry - _length ) / 2;
        int l_py = ( l_terminalHeight - _x ) / 2;

        /* Process each row of the canvas. */
        for ( int l_i = 0; l_i < _x; l_i++ ) {
            /* Length of the current art row in the source frame. */
            int l_len = _art[ l_currentFrame ][ l_i ].size();

            /* Walk across the full output width, including extra text space. */
            for ( int l_j = 0; l_j < _y + _length; l_j++ ) {
                /* If this column is still inside the source art row, copy the
                   source character into the canvas. */
                if ( l_j < l_len ) {
                    l_canvas[ l_i ][ l_j ] =
                        _art[ l_currentFrame ][ l_i ][ l_j ];

                    /* Otherwise, if the destination cell is still empty, fill
                       it with a space so the row remains printable as a string.
                     */
                } else if ( l_canvas[ l_i ][ l_j ] == '\0' ) {
                    l_canvas[ l_i ][ l_j ] = ' ';
                }

                /* Draw the decorative connector only on the first art row,
                   and only if there is extra space reserved for it. */
                if ( !l_i && _length ) {
                    /* At the start of the extra area, place the opening slash
                       pair that begins the connector outline. */
                    if ( l_j == _y ) {
                        l_canvas[ l_pt1-- ][ l_j ] = '/';
                        l_canvas[ l_pt2++ ][ l_j ] = '\\';

                        /* One column before the opening edge, draw vertical
                           bars extending from both connector sides. */
                    } else if ( l_j - 1 == _y ) {
                        for ( int l_n = 0; l_n < _lines / 2; l_n++ ) {
                            l_canvas[ l_pt1-- ][ l_j ] = '|';
                            l_canvas[ l_pt2++ ][ l_j ] = '|';
                        }

                        /* Undo the last lower-side increment so the next stage
                           stays aligned. */
                        l_pt2--;

                        /* At the far edge of the extra area, close the
                           connector with vertical bars between the two sides.
                         */
                    } else if ( l_j + 1 == _y + _length ) {
                        for ( int l_k = ++l_pt1; l_k <= l_pt2; l_k++ ) {
                            l_canvas[ l_k ][ l_j ] = '|';
                        }

                        /* Restore l_pt1 so the next row starts at the intended
                           vertical position. */
                        l_pt1++;

                        /* Everywhere else in the connector area, draw the top
                           and bottom outline with underscores. */
                    } else {
                        l_canvas[ l_pt1 ][ l_j ] = '_';
                        l_canvas[ l_pt2 ][ l_j ] = '_';
                    }
                }
            }

            /* Write the text only once, on the first row that reaches the
               text-rendering section. */
            if ( !l_cnt && ( _length || _lines ) ) {
                /* Start writing text from the current connector anchor. */
                int l_textRow = l_pt1;
                int l_textCol = l_ptt;

                /* Copy the string_view character by character. */
                for ( char l_ch : _text ) {
                    /* Newline forces an explicit row break and resets the
                       column to the initial text anchor. */
                    if ( l_ch == '\n' ) {
                        l_textRow++;
                        l_textCol = l_ptt;
                        l_cnt = 0;
                        continue;
                    }

                    /* Wrap to the next output row once the current line reaches
                       the configured maximum width. */
                    if ( l_cnt >= MAX_LENGTH ) {
                        l_textRow++;
                        l_textCol = l_ptt;
                        l_cnt = 0;
                    }

                    /* Write the current character into the canvas. */
                    l_canvas[ l_textRow ][ l_textCol++ ] = l_ch;
                    l_cnt++;
                }
            }

            /* Mark that at least one canvas row has been processed.
               This prevents the text block from being written again. */
            l_cnt = 1;
        }

        /* Print the completed canvas at the computed terminal position. */
        printCanvas( l_canvas, _x, l_px, l_py );

        /* Sleep for the current frame delay, then advance the frame index. */
        usleep( _intervals[ l_currentFrame++ ] );

        /* Free the canvas memory allocated for this frame. */
        freeCanvas( l_canvas );

        /* When the last frame has been shown, loop back to the first one. */
        if ( l_currentFrame == _frames ) {
            l_currentFrame = 0;

            /* Decrement rounds only when it is positive.
               Negative values are treated as "run forever". */
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

            constructV1( art::g_momoiAnimatedV1, _text, l_frame, 5,
                         ANIMATED_V1_X, ANIMATED_V1_Y, ANIMATED_V1_RY, l_length,
                         l_lines, -1 );
        }

    } else if ( g_mode == detail::mode_t::animated ) {
        if ( l_lines <= 10 ) {
            if ( l_lines & 1 ) {
                l_lines++;
            }

            constexpr std::array l_frame = { 75000 };

            constructV1( art::g_momoiStaticV1, _text, l_frame, 1, STATIC_V1_X,
                         STATIC_V1_Y, STATIC_V1_RY, l_length, l_lines, -1 );
        }
    }
}

} // namespace momoisay

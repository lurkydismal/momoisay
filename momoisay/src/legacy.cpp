#include <getopt.h>
#include <ncurses.h>
#include <unistd.h>

#include <charconv>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "art.hpp"
#include "momoisay.hpp"
#include "stdrandom.hpp"

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

static void init() {
    setlocale( LC_ALL, "" );
    initscr();
    cbreak();
    noecho();
    keypad( stdscr, TRUE );
    curs_set( 0 );
    timeout( -1 );
}

static void help() {
    printf(
        "Make cute Momoi from Blue Archive say something!!!\n"
        "operations:\n"
        "    -h, --help                          Display this help message\n"
        "    -v, --version                       Show version information\n"
        "    -a <version> <text>                 Cool animated version of "
        "cute Momoi (default version 1)\n"
        "    -f <text>                           Freestyle Momoi animation\n"
        "    -s <version> <text>                 Display static version of "
        "cute Momoi (default version 1)\n"
        "    -l, --list                          List available versions for "
        "Momoi ASCII arts\n"
        "    <text>                              Text that cute Momoi will "
        "say!!! (default static version 1)\n" );
}

auto stoi( char* _s ) -> int {
    const char* l_first = _s;
    const auto l_last = std::end( std::span( _s, strlen( _s ) ) );
    int l_value = 0;

    std::from_chars( l_first, l_last.base(), l_value );

    return l_value;
}

static auto randomizer( int _min, int _max ) -> int {
    return stdfunc::random::number::weak( _min, _max );
}

static auto randint( const int _arr[], int _size ) -> int {
    return stdfunc::random::value( std::span( _arr, _size ) );
}

static auto getLine( char* _argv[], int _start, int _end ) -> int {
    int l_lines = 0, l_cnt = 0;
    if ( _end - _start )
        l_lines++;
    for ( int l_i = _start; l_i < _end; l_i++ ) {
        char* l_str = _argv[ l_i ];
        while ( *l_str != '\0' ) {
            if ( l_cnt >= MAX_LENGTH ) {
                l_cnt = 0;
                l_lines++;
            }
            l_cnt++;
            l_str++;
        }
        l_cnt++;
    }
    return l_lines;
}

static auto createCanvas( int _x, int _y ) -> char** {
    char** l_canvas = ( char** )malloc( _x * sizeof( char* ) );
    for ( int l_i = 0; l_i < _x; l_i++ ) {
        l_canvas[ l_i ] = ( char* )calloc( _y + 1, sizeof( char ) );
    }
    return l_canvas;
}

static void printCanvas( char** _canvas, int _x, int _px, int _py ) {
    for ( int l_i = 0; l_i < _x; l_i++ ) {
        mvprintw( _py + l_i, _px, "%s", _canvas[ l_i ] );
    }
    refresh();
}

static void freeCanvas( char** _canvas, int _x ) {
    if ( _canvas == nullptr )
        return;
    for ( int l_i = 0; l_i < _x; l_i++ ) {
        free( _canvas[ l_i ] );
    }
    free( _canvas );
}

static auto textlen( char* _argv[], int _start, int _end ) -> int {
    int l_length = 0;
    for ( int l_i = _start; l_i < _end; l_i++ )
        l_length += strlen( _argv[ l_i ] ) + 1;
    if ( l_length - 1 > MAX_LENGTH )
        return MAX_LENGTH;
    return l_length - 1;
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
                    if ( l_cnt > MAX_LENGTH )
                        continue;
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
            if ( _round > 0 )
                _round--;
        }
    }
}

static void constructV2( std::span< const art::frame_t > _art,
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
                         int _reped,
                         int _repmin,
                         int _repmax,
                         int _round ) {
    int l_currentFrame = 0, l_replap = randomizer( _repmin, _repmax );
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
            int l_len = _art[ l_currentFrame ][ l_i ].length();
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
                    if ( l_cnt > MAX_LENGTH )
                        continue;
                    l_canvas[ l_pt1 ][ l_pts++ ] = ' ';
                    l_cnt++;
                }
            }
            l_cnt = 1;
        }
        printCanvas( l_canvas, _x, l_px, l_py );
        usleep( _intervals[ l_currentFrame ] );
        if ( l_currentFrame == _reped ) {
            if ( l_replap == 0 ) {
                l_replap = randomizer( _repmin, _repmax );
                l_currentFrame++;
            } else {
                l_currentFrame = 0;
                l_replap--;
            }
        } else {
            l_currentFrame++;
        }
        freeCanvas( l_canvas, _x );
        if ( l_currentFrame == _frames ) {
            l_currentFrame = 0;
            if ( _round > 0 )
                _round--;
        }
    }
}

static void constructFreestyle( char* _argv[],
                                int _length,
                                int _lines,
                                int _start,
                                int _end ) {
    int l_select = randomizer( 0, ANIMATED_VERSION - 1 );
    while ( true ) {
        if ( l_select == 0 ) {
            int l_frame[ 5 ] = { 150000, 75000, 150000, 150000, 75000 };
            constructV1( art::g_momoiAnimatedV1, _argv, l_frame, 5,
                         ANIMATED_V1_X, ANIMATED_V1_Y, ANIMATED_V1_RY, _length,
                         _lines, _start, _end, randomizer( 3, 5 ) );
            l_select = 2;
        } else if ( l_select == 1 ) {
            int l_frame[ 7 ] = { 70000, 70000, 70000, 1500000,
                                 70000, 70000, 70000 };
            constructV2( art::g_momoiAnimatedV2, _argv, l_frame, 7,
                         ANIMATED_V2_X, ANIMATED_V2_Y, ANIMATED_V2_RY, _length,
                         _lines, _start, _end, 1, 5, 13, randomizer( 1, 3 ) );
            l_select = randint( ( int[] ){ 0, 2 }, 2 );
        } else if ( l_select == 2 ) {
            int l_frame[ 8 ] = { 70000, 70000, 70000, 70000,
                                 70000, 70000, 70000, 70000 };
            constructV1( art::g_momoiAnimatedV3, _argv, l_frame, 8,
                         ANIMATED_V3_X, ANIMATED_V3_Y, ANIMATED_V3_RY, _length,
                         _lines, _start, _end, randomizer( 3, 5 ) );
            l_select = randint( ( int[] ){ 1 }, 1 );
        }
    }
}

namespace momoisay {

auto setVersion( [[maybe_unused]] int _key,
                 [[maybe_unused]] std::string_view _value,
                 [[maybe_unused]] arhodigp::state_t _state ) -> bool {
    return ( true );
}

auto run( int _argumentCount, char** _argumentVector ) -> int {
    srand( time( nullptr ) );

    int l_option = 0;
    int l_mode = 0;
    int l_ctl = 0;
    int l_argctl = 0;
    int l_animatedVersion = 1;
    int l_staticVersion = 1;

    constexpr auto l_longOptions = std::to_array< option >(
        { { "help", no_argument, nullptr, 'h' },
          { "version", no_argument, nullptr, 'v' },
          { "list", no_argument, nullptr, 'l' },
          { .name = nullptr, .has_arg = 0, .flag = nullptr, .val = 0 } } );

    while ( ( l_option = getopt_long( _argumentCount, _argumentVector,
                                      "hvla::s::f::", l_longOptions.data(),
                                      nullptr ) ) != -1 ) {
        switch ( l_option ) {
            case 'l':
                printf( "static: " );
                for ( int l_i = 1; l_i <= STATIC_VERSION; l_i++ ) {
                    printf( "%d ", l_i );
                }
                printf( "\n" );
                printf( "animated: " );
                for ( int l_i = 1; l_i <= ANIMATED_VERSION; l_i++ ) {
                    printf( "%d ", l_i );
                }
                printf( "\n" );
                return 0;
            case 'a':
                l_mode = 1;
                if ( !l_ctl )
                    l_argctl = 0;
                if ( _argumentCount <= 2 )
                    break;
                optarg = _argumentVector[ 2 ];
                if ( optarg && 0 < stoi( optarg ) &&
                     stoi( optarg ) <= ANIMATED_VERSION && !l_ctl ) {
                    l_ctl = 1;
                    l_argctl = 1;
                    l_animatedVersion = stoi( optarg );
                }
                break;
            case 's':
                l_mode = 0;
                if ( !l_ctl )
                    l_argctl = 0;
                if ( _argumentCount <= 2 )
                    break;
                optarg = _argumentVector[ 2 ];
                if ( optarg && 0 < stoi( optarg ) &&
                     stoi( optarg ) <= STATIC_VERSION && !l_ctl ) {
                    l_ctl = 1;
                    l_argctl = 1;
                    l_staticVersion = stoi( optarg );
                }
                break;
            case 'f':
                l_mode = 2;
                if ( !l_ctl )
                    l_argctl = 0;
                if ( _argumentCount <= 2 )
                    break;
                optarg = _argumentVector[ 2 ];
                break;
            default:
                help();
                return 0;
        }
    }
    init();
    if ( l_mode == 2 ) {
        int l_length = 0, l_lines = 0;
        l_length =
            5 + textlen( _argumentVector, optind + l_argctl, _argumentCount );
        l_lines = getLine( _argumentVector, optind + l_argctl, _argumentCount );
        if ( l_length <= 5 )
            l_length = 0;
        if ( l_lines <= 10 ) {
            if ( l_lines & 1 )
                l_lines++;
            constructFreestyle( _argumentVector, l_length, l_lines,
                                optind + l_argctl, _argumentCount );
            return 0;
        }
    } else if ( l_mode == 1 ) {
        int l_length = 0, l_lines = 0;
        l_length =
            5 + textlen( _argumentVector, optind + l_argctl, _argumentCount );
        l_lines = getLine( _argumentVector, optind + l_argctl, _argumentCount );
        if ( l_animatedVersion == 1 ) {
            if ( l_length <= 5 )
                l_length = 0;
            if ( l_lines <= 30 ) {
                if ( l_lines & 1 )
                    l_lines++;
                constexpr std::array l_frame = { 150000, 75000, 150000, 150000,
                                                 75000 };
                constructV1( art::g_momoiAnimatedV1, _argumentVector, l_frame,
                             5, ANIMATED_V1_X, ANIMATED_V1_Y, ANIMATED_V1_RY,
                             l_length, l_lines, optind + l_argctl,
                             _argumentCount, -1 );
            }
        } else if ( l_animatedVersion == 2 ) {
            if ( l_length <= 5 )
                l_length = 0;
            if ( l_lines <= 30 ) {
                if ( l_lines & 1 )
                    l_lines++;
                constexpr std::array l_frame = { 70000, 70000, 70000, 1500000,
                                                 70000, 70000, 70000 };
                constructV2( art::g_momoiAnimatedV2, _argumentVector, l_frame,
                             7, ANIMATED_V2_X, ANIMATED_V2_Y, ANIMATED_V2_RY,
                             l_length, l_lines, optind + l_argctl,
                             _argumentCount, 1, 5, 13, -1 );
            }
        } else if ( l_animatedVersion == 3 ) {
            if ( l_length <= 5 )
                l_length = 0;
            if ( l_lines <= 30 ) {
                if ( l_lines & 1 )
                    l_lines++;
                constexpr std::array l_frame = { 70000, 70000, 70000, 70000,
                                                 70000, 70000, 70000, 70000 };
                constructV1( art::g_momoiAnimatedV3, _argumentVector, l_frame,
                             8, ANIMATED_V3_X, ANIMATED_V3_Y, ANIMATED_V3_RY,
                             l_length, l_lines, optind + l_argctl,
                             _argumentCount, -1 );
            }
        }
    } else if ( l_mode == 0 ) {
        if ( l_staticVersion == 1 ) {
            int l_length = 0, l_lines = 0;
            l_length = 5 + textlen( _argumentVector, optind + l_argctl,
                                    _argumentCount );
            l_lines =
                getLine( _argumentVector, optind + l_argctl, _argumentCount );
            if ( l_length <= 5 )
                l_length = 0;
            if ( l_lines <= 10 ) {
                if ( l_lines & 1 )
                    l_lines++;
                constexpr std::array l_frame = { 75000 };
                constructV1( art::g_momoiStaticV1, _argumentVector, l_frame, 1,
                             STATIC_V1_X, STATIC_V1_Y, STATIC_V1_RY, l_length,
                             l_lines, optind + l_argctl, _argumentCount, -1 );
                return 0;
            }
        }
    }

    return 0;
}

} // namespace momoisay

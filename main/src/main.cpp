#include <cstdlib>
#include <print>
#include <span>

#include "arhodigp.hpp"
#include "momoisay.hpp"
#include "stdfunc.hpp"

#if defined( __SANITIZE_LEAK__ )

#include <sanitizer/lsan_interface.h>

#endif

auto main( int _argumentCount, char** _argumentVector ) -> int {
    std::println( "{}: '{}'", _argumentCount,
                  std::span( _argumentVector, _argumentCount ) );

    std::map< int, arhodigp::option_t > l_options{
        {
            'a',
            { "animated", momoisay::setAnimated, "",
              "Cool animated version of cute Momoi" },
        },
        { 's',
          { "static", momoisay::setStatic, "",
            "Display static version of cute Momoi" } },
    };

    arhodigp::parseArguments(
        "",
        stdfunc::spanToVector< char*, std::string_view >(
            std::span( _argumentVector, _argumentCount ) ),
        "momoisay", "Make cute Momoi from Blue Archive say something!!!", 0.1f,
        "github.com/lurkydismal/momoisay", l_options );

    momoisay::run( _argumentCount, _argumentVector );

#if defined( __SANITIZE_LEAK__ )

    __lsan_do_leak_check();

#endif

    return ( EXIT_SUCCESS );
}

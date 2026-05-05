#include <cstdlib>
#include <span>

#include "arhodigp.hpp"
#include "momoisay.hpp"
#include "stdfunc.hpp"

#if defined( __SANITIZE_LEAK__ )

#include <sanitizer/lsan_interface.h>

#endif

auto main( int _argumentCount, char** _argumentVector ) -> int {
    std::string l_text = "";

    arhodigp::callback_t l_acceptText =
        [ & ]( [[maybe_unused]] int _key, std::string_view _value,
               [[maybe_unused]] arhodigp::state_t _state ) -> bool {
        l_text = _value;

        return ( true );
    };

    std::map< int, arhodigp::option_t > l_options{
        {
            'a',
            { "animated", momoisay::setAnimated, "",
              "Cool animated version of cute Momoi" },
        },
        { 's',
          { "static", momoisay::setStatic, "",
            "Display static version of cute Momoi" } },
        { arhodigp::key_t::positionalArgument | 0u,
          { "", l_acceptText, "[text]", "Text that cute Momoi will say" } },
    };

    arhodigp::parseArguments(
        "",
        stdfunc::spanToVector< char*, std::string_view >(
            std::span( _argumentVector, _argumentCount ) ),
        "momoisay", "Make cute Momoi from Blue Archive say something!!!", 0.1f,
        "github.com/lurkydismal/momoisay", l_options );

    momoisay::oneshot( l_text );

#if defined( __SANITIZE_LEAK__ )

    __lsan_do_leak_check();

#endif

    return ( EXIT_SUCCESS );
}

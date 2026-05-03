#include <cstdlib>
#include <print>
#include <span>

#include "arhodigp.hpp"
#include "logg.hpp"
#include "momoisay.hpp"
#include "stddebug.hpp"
#include "stdfunc.hpp"

#if defined( __SANITIZE_LEAK__ )

#include <sanitizer/lsan_interface.h>

#endif

auto main( int _argumentCount, char** _argumentVector ) -> int {
    std::println( "{}: '{}'", _argumentCount,
                  std::span( _argumentVector, _argumentCount ) );

    arhodigp::callback_t l_nothing =
        [ & ]( [[maybe_unused]] int _key,
               [[maybe_unused]] std::string_view _value,
               [[maybe_unused]] arhodigp::state_t _state ) -> bool {
        logg$variable( _key );
        logg$variable( _value );
        logg$variable( _state.get() );

        return ( true );
    };

    arhodigp::callback_t l_error =
        [ & ]( [[maybe_unused]] int _key,
               [[maybe_unused]] std::string_view _value,
               [[maybe_unused]] arhodigp::state_t _state ) -> bool {
        logg$variable( _key );
        logg$variable( _value );
        logg$variable( _state.get() );

        return ( false );
    };

    arhodigp::callback_t l_errorAnother =
        [ & ]( [[maybe_unused]] int _key,
               [[maybe_unused]] std::string_view _value,
               arhodigp::state_t _state ) -> bool {
        logg$variable( _key );
        logg$variable( _value );
        logg$variable( _state.get() );

        arhodigp::error( _state, "No message" );

        return ( true );
    };

    arhodigp::callback_t l_terminate =
        [ & ]( [[maybe_unused]] int _key,
               [[maybe_unused]] std::string_view _value,
               [[maybe_unused]] arhodigp::state_t _state ) -> bool {
        logg$variable( _key );
        logg$variable( _value );
        logg$variable( _state.get() );

        logg::warning( "Terminating process" );

        stdfunc::trap();

        return ( true );
    };

    std::map< int, arhodigp::option_t > l_options{
        {
            'a',
            { "alternative", momoisay::setVersion, "VERSION",
              "Cool animated version of cute Momoi" },
        },
        { 'n', { "nothing", l_nothing } },
        { 'e',
          {
              "",
              l_error,
              "No arguments",
              "Will return false from it's callback",
          } },
        { 'E',
          {
              "",
              l_errorAnother,
              "",
              "Will call arhodigp::error() from it's callback",
          } },
        { 'i', { "input", l_nothing, "argument" } },
        { 't',
          {
              "terminate",
              l_terminate,
              "",
              "Will call stdfunc::trap() from it's callback",
          } },
    };

    arhodigp::parseArguments(
        "",
        stdfunc::spanToVector< char*, std::string_view >(
            std::span( _argumentVector, _argumentCount ) ),
        "momoisay", "Make cute Momoi from Blue Archive say something!!!", 0.1f,
        "github.com/lurkydismal/momoisay", l_options );

    momoisay::printBuildType();

    logg$debug( "RUN: {}", momoisay::run( _argumentCount, _argumentVector ) );

#if defined( __SANITIZE_LEAK__ )

    __lsan_do_leak_check();

#endif

    logg$error( "SUCCESS" );

    return ( EXIT_FAILURE );
}

//
// Created by Jan on 07/09/2023.
//

#ifndef JIO_TEST_COMMON_H
#define JIO_TEST_COMMON_H

#ifndef NDEBUG
    #ifdef __GNUC__
        #define DBG_STOP __builtin_trap()
    #endif
    #ifdef _WIN32
        #define DBG_STOP __debugbreak()
    #endif
#endif
#ifndef DBG_STOP
    #define DBG_STOP (void)0
#endif


#define ASSERT(x) if (!(x)) {fputs("Failed assertion \"" #x "\"\n", stderr); DBG_STOP; exit(EXIT_FAILURE);} (void)0

#endif //JIO_TEST_COMMON_H

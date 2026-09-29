#ifndef _CIRCNEWLIB_CIRCLENETMAP_H
#define _CIRCNEWLIB_CIRCLENETMAP_H

namespace _CircleStdlib
{
    /**
     * Wrap Circle's socket-related defines
     *
     * Circle uses the same names and even the same numeric
     * values as Newlib for socket-related defines. In order to
     * rely on this identity Circle's defines are wrapped here.
     */
    class CircleNetMap
    {
    public:
        static int const C_IPPROTO_TCP;
        static int const C_IPPROTO_UDP;
        static int const C_MSG_DONTWAIT;
    };
}

#endif
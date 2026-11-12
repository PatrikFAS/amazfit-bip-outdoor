/* Keep BipOS entry points and SDK symbols visible. These internal C runtime
   names are resolved by the static link, so they can be discarded afterward. */
{
    global: *;
    local:
        __aeabi_*;
        __adddf3; __subdf3; __muldf3; __divdf3;
        __eqdf2; __nedf2; __ledf2; __ltdf2; __gedf2; __gtdf2; __unorddf2;
        __floatsidf; __floatunsidf; __floatdidf; __floatundidf;
        __fixdfsi; __extendsfdf2; __truncdfsf2;
        memcpy; memset;
};

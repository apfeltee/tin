
        static void* dispatchtable[] =
        {
            #define OPCODE(name, effect) &&name,
            #include "opcodes.inc"
            #undef OPCODE
        };

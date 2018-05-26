#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * Compiler quirks
 */
#ifdef __VBCC__
#define INTERRUPT __amigainterrupt __saveds
#define LIBFUNC __saveds
#define REG(reg, arg) __reg(#reg) arg
#ifdef _DEBUG_
#define DBG_PRINT(...) kprintf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif
#endif

#ifdef __SASC
 #define INTERRUPT __interrupt __saveds __asm
 #define LIBFUNC __saveds __asm
 #define REG(reg,arg) register __## reg arg

 #ifdef _DEBUG_
  #undef DBG_PRINT
  #define DBG_PRINT kprintf
 #else
  #define DBG_PRINT /##/; 
 #endif
#endif

#define _INTTOSTR(a) #a
#define INTTOSTR(a) _INTTOSTR(a)

#endif

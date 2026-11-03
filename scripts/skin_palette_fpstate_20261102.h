/* The memoized pair uses only FP_ST loads/stores/arithmetic and push/pop:
 * these do not consult virtual CW. Keep masked exceptions, reject reserved
 * precision, and key the full virtual word; reserved bit6 may be clear. */
static int skin_guest_cw_supported(uint16_t cw){return (cw&0x3f)==0x3f && (cw&0x300)!=0x100;}
/* Full native FP controls and per-unit status: no floating arithmetic. */
/* Pure predicate: test synthetic controls without enabling actual traps. */
static int skin_host_fp_masked_control(uint64_t control){
#if defined(__aarch64__)
 return !(control & 0x9f00u); /* IDE, IXE, UFE, OFE, DZE, IOE */
#elif defined(__x86_64__) || defined(__i386__)
 return (control & 0x1f80u)==0x1f80u && ((control>>32)&0x3fu)==0x3fu;
#else
 return 0;
#endif
}
static int skin_host_fp(uint64_t *control,uint64_t *status){
#if defined(__aarch64__)
 __asm__ volatile("mrs %0, fpcr" : "=r"(*control));
 __asm__ volatile("mrs %0, fpsr" : "=r"(*status));return skin_host_fp_masked_control(*control);
#elif defined(__x86_64__) || defined(__i386__)
 uint32_t mx;uint16_t cw,sw;
 __asm__ volatile("stmxcsr %0" : "=m"(mx));
 __asm__ volatile("fnstcw %0" : "=m"(cw));
 __asm__ volatile("fnstsw %0" : "=am"(sw));
 *control=(uint64_t)(mx&~63u)|((uint64_t)cw<<32);
 *status=(uint64_t)(mx&63u)|((uint64_t)sw<<32);return skin_host_fp_masked_control(*control);
#else
 *control=*status=0;return 0;
#endif
}

#define CR0_PE		 1u
#define CR0_MP		(1u << 1)
#define CR0_EM		(1u << 2)
#define CR0_TS		(1u << 3)
#define CR0_ET		(1u << 4)
#define CR0_NE		(1u << 5)
#define CR0_WP		(1u << 16)
#define CR0_AM		(1u << 18)
#define CR0_NW		(1u << 29)
#define CR0_CD		(1u << 30)
#define CR0_PG		(1u << 31)

#define CR4_VME		 1u
#define CR4_PVI		(1u << 1)
#define CR4_TSD		(1u << 2)
#define CR4_DE		(1u << 3)
#define CR4_PSE		(1u << 4)
#define CR4_PAE		(1u << 5)
#define CR4_MCE		(1u << 6)
#define CR4_PGE		(1u << 7)
#define CR4_PCE		(1u << 8)
#define CR4_OSFXSR	(1u << 8)
#define CR4_OSXMMEXCPT	(1u << 10)
#define CR4_UMIP	(1u << 11)
#define CR4_VMXE	(1u << 13)
#define CR4_SMXE	(1u << 14)
#define CR4_FSGSBASE	(1u << 16)
#define CR4_PCIDE	(1u << 17)
#define CR4_OSXSAVE	(1u << 18)
#define CR4_SMEP	(1u << 20)
#define CR4_SMAP	(1u << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)


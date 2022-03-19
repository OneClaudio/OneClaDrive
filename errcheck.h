/* simplified version of ec.c/ec.h inspired from AUP book (Rochkind) */

extern const bool ErrInCLEANUP=false;

#define ErrCLEANUP				\
	ErrWARN();					\
								\
	ErrCleanup:					\
		{						\
		bool ErrInCLEANUP;		\
		ErrInCLEANUP=true;


#define ErrCLEAN				\
		}
		

#define ErrCMPR( rv , errv)																							\
	{																												\
		assert( !ErrInCLEANUP );																					\
		errno=0;																									\
		if( (rv)==(errv) ){																							\
			int errnocpy=errno;																						\
			fprintf(stderr,"ERROR: by %s (%s:%d)\n   %s returned: %s\t%s (ERRNO=%d)\n",							\
					__func__, __FILE__, __LINE__, #rv, #errv, strerror(errnocpy), errnocpy);						\
			goto ErrCleanup;																						\
		}

#define ErrNEG1( rv )	ErrCMPR( rv, -1)

#define ErrNULL( rv )	ErrCMPR( rv, NULL)

#define ErrFALSE(rv )	ErrCMPR( rv, false)

#define ErrZERO( rv )	ErrCMPR( rv, 0)

#define ErrNZERO(rv )	ErrDIFF( rv, 0)

#define ErrDIFF( rv , corrv)				\
	{										\
		if( (rv) != (corrv) )				\
			ErrCMPR(0, 0) /*FAIL*/			\
		}

#define SUCCESS

void ErrWARN(){
	fprintf(stderr, "/!\ Warning: Control reached ErrCLEANUP section\n");
	}

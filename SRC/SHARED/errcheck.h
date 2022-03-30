/* simplified version of ec.c/ec.h inspired from AUP book (Rochkind) */

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
//include <assert.h>

//extern const bool ErrInCLEANUP;	//needs an errcheck.c file with its definition: const bool ErrInCLEANUP=false;
									//WARNING: this was used to prevent infinite loops when using these macros from within the cleanup section
										//Removing this because raises warnings for not being used. Be careful not to commit this mistake ^^^
										
//BEGINNING of the CLEANUP SECTION
#define ErrCLEANUP				\
	ErrWARN;					\
								\
	ErrCleanup:					\
		{						
	/*	bool ErrInCLEANUP;	
		ErrInCLEANUP=true;	*/

//END of the CLEANUP SECTION
#define ErrCLEAN				\
		}

#define ErrLOCAL __label__ ErrCleanup;
//allows to declare nested GOTO LABELS with the same name, a much needed feature
//	without this the same LABEL is allowed only ONCE in each FUNCTION
//	with this each BLOCK that has 'ErrLOCAL' as the first definition, can have a LOCAL LABEL, even if the same label is already present outside
//	/!\ needs the gnu99 standard (an expanded c99)  ->  COMPILE with -std=gnu99


#define ErrCMPR( rv , errv)																		\
	{/*	assert( !ErrInCLEANUP );*/																\
		errno=0;																				\
		if( (rv)==(errv) ){																		\
			int errnocpy=errno;																	\
			fprintf(stderr,"ERROR: by %s (%s:%d)\n   %s returned: %s\t%s (ERRNO=%d)\n",			\
					__func__, __FILE__, __LINE__, #rv, #errv, strerror(errnocpy), errnocpy);	\
			goto ErrCleanup;																	\
			}																					\
		}

#define ErrNEG1( rv )	ErrCMPR( rv, -1)

#define ErrNULL( rv )	ErrCMPR( rv, NULL)

#define ErrFALSE(rv )	ErrCMPR( rv, false)

#define ErrZERO( rv )	ErrCMPR( rv, 0)

#define ErrNZERO(rv )	ErrDIFF( rv, 0)

#define ErrDIFF( rv , corrv)				\
	{	errno=0;							\
		if( (rv) != (corrv) )				\
			ErrFAIL							\
		}
	
#define ErrERRNO( rv) {																		\
		int errcd=0;																		\
		if( (errcd=(rv)) !=0 ){																\
			fprintf(stderr,"ERROR: by %s (%s:%d)\n   %s returned: ERRNO=%d  %s\n",			\
						__func__, __FILE__, __LINE__, #rv, errcd, strerror(errcd) );		\
			goto ErrCleanup;																\
			}																				\
		}

#define ErrFAIL							/*Always FAILS   Only prints ERR MSG		*/										\
	{	fprintf(stderr,"ERROR: by %s (%s:%d)\t%s (ERRNO=%d)\n",__func__, __FILE__, __LINE__, strerror(errno), errno);	\
		goto ErrCleanup;																								\
		}

#define ErrSHHH goto ErrCleanup;		/*Always FAILS   Like ErrFAIL but SILENT	*/

#define ErrWARN fprintf(stderr, "Warning: Control reached the error cleanup section\n");

#define SUCCESS

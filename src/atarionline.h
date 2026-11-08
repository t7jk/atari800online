#ifndef ATARIONLINE_H_
#define ATARIONLINE_H_

#include "config.h"

#ifdef HAVE_DOWNLOAD

/* Interactive browser of the atarionline.pl game catalogue (letter -> title
   -> file). The chosen file is downloaded, unpacked if needed and started
   as with "Run Atari Program". Returns TRUE if a program was started
   (the UI should then be left), FALSE otherwise. */
int ATARIONLINE_Browse(void);

/* Menu of games previously started from the browser (works offline).
   Returns TRUE if a program was started. */
int ATARIONLINE_History(void);

#endif /* HAVE_DOWNLOAD */

#endif /* ATARIONLINE_H_ */

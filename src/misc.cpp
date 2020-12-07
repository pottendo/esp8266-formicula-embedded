#include <Arduino.h>
#include "defs.h"

/* helpers */
void log_msg(const char *s)
{
#if 0
	P(ui_mutex);
	lv_textarea_add_text(log_handle, s);
	lv_textarea_add_char(log_handle, '\n');
	V(ui_mutex);
#endif
	printf("%s\n", s);
}

void log_msg(const String &s)
{
	log_msg(s.c_str());
}

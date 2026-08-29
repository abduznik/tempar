#ifndef MAIN_H
#define MAIN_H

void button_callback(u32 curr_but, u32 last_but, void *arg);
void gamePause(SceUID thread_id);
void gameResume(SceUID thread_id);
int module_stop(int argc, char *argv[]);

#endif
#include "log.h"
#include <stdio.h>

int main(void)
{
    Log_Handle my_log;
    size_t num;

    const log_entry second_entry =
        "hello "
        "draganxxxxaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccdddddddddddd"
        "ddddeeeeeeeeeeeeeeeeffffffffffffffffgggggggggggggggghhhhhhhhhhhhhhhhii"
        "iiiiiiiiiiiiiijjjjjjjjjjjjjjjjkkkkkkkkkkkkkkkkllllllllllllllllmmmmmmmm"
        "mmmmmmmmnnnnnnnnnnnnnnnnoooooooooooooooo"; // 256 chars not including final \0

    Log_IO_Initialize();

    create_log(&my_log, "test2log.txt", HTTP_Endpoint_None);

    write_entry(&my_log, second_entry);

    num = Log_IO_Num_Base64_Entries(&my_log);

    printf("Num entries in the files is %d\n", (int)num);

    Log_IO_Close(&my_log);
}

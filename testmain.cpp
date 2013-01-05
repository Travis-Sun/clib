// -*- coding:gb2312 -*-
//#include <cstdio>
//#include <cstdlib>
//#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "times33hash.h"
#include <iostream>
#include "win.h"
#include <locale.h>

#define UNICODE
#define _UNICODE

using namespace std;

int main(void)
{
    setlocale(LC_ALL, "chs");
    FILE *fh;    
    S_CHAR *key = "i am student";
    S_WCHAR *key2 = L"是非成败转头空123";
        
    fh = fopen("test.log","a");
    printf("key=%s, the hash value is %d\n", key, Times33Hash::hash(key, strlen(key)));
    wprintf(L"key=%s, the hash value is %ud\n", key2, Times33Hash::hash(key2, wcslen(key2)));
    wprintf(L"test\n");
    //fwprintf(fh, key2);
    wcout<<key2<<endl;
    fwprintf(fh, L"key=%s, the hash value is %d\n", key2, Times33Hash::hash(key2, wcslen(key2)));
    
    fclose(fh);
    return 0;
}

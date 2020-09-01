#include "cli.h"
#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "vm.h"
#include "core.h"

static void runFile(const char* path) {
    const char* lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {
        char* root = (char*)malloc(lastSlash - path + 2);
        memcpy(root, path, lastSlash - path + 1);
        root[lastSlash - path + 1] = '\0';
        rootDir = root;
    }

    VM* vm = newVM();
    const char* sourceCode = readFile(path);

   //  struct parser parser;
   //  initParser(vm, &parser, path, sourceCode, NULL);
   //
   //  #include "token.list"
   //  while (parser.curToken.type != TOKEN_EOF) {
   //    getNextToken(&parser);
   //    printf("%dL: %s [", parser.curToken.lineNo, tokenArray[parser.curToken.type]);
   //    // if (parser.curToken.type == 54) {
   //    //   printf("\n 52 is comming %d %s \n", parser.curToken.type - 2, tokenArray[parser.curToken.type-2]);
   //    //   printf("\n 53 is comming %d %s \n", parser.curToken.type - 1, tokenArray[parser.curToken.type-1]);
   //    //   printf("\n 54 is comming %d %s \n", parser.curToken.type, tokenArray[parser.curToken.type]);
   //    // }
   //    uint32_t idx = 0;
   //    while (idx < parser.curToken.length) {
	 // printf("%c", *(parser.curToken.start+idx++));
   //    }
   //    printf("]\n");
   //  }
    // int i = 0;
    //  while (i < 55) {
    //   printf(" %d is comming %d %s \n", i, i, tokenArray[i]);
    //   i++;
    // }

    executeModule(vm, OBJ_TO_VALUE(newObjString(vm, path, strlen(path))), sourceCode);

}



int main(int argc, const char** argv) {
   if (argc == 1) {
      ;
   } else {
      runFile(argv[1]);
   }
   return 0;
}

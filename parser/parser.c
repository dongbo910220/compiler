#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "utils.h"
#include "unicodeUtf8.h"
#include <string.h>
#include "obj_string.h"
#include <ctype.h>

struct keywordToken {
    char*       keyword;
    uint8_t     length;
    TokenType   token;
};

struct keywordToken keywordsToken[] = {
   {"var",	  3,	TOKEN_VAR},
   {"fun",	  3,	TOKEN_FUN},
   {"if",	  2,	TOKEN_IF},
   {"else",	  4,  	TOKEN_ELSE},
   {"true",	  4,  	TOKEN_TRUE},
   {"false",	  5,  	TOKEN_FALSE},
   {"while",	  5,  	TOKEN_WHILE},
   {"for",	  3,  	TOKEN_FOR},
   {"break",	  5,  	TOKEN_BREAK},
   {"continue",   8,    TOKEN_CONTINUE},
   {"return",	  6,  	TOKEN_RETURN},
   {"null",	  4,  	TOKEN_NULL},
   {"class",	  5,  	TOKEN_CLASS},
   {"is",	  2,  	TOKEN_IS},
   {"static",	  6,  	TOKEN_STATIC},
   {"this",	  4,  	TOKEN_THIS},
   {"super",	  5,  	TOKEN_SUPER},
   {"import",	  6,  	TOKEN_IMPORT},
   {NULL,	  0,  	TOKEN_UNKNOWN}
};

static TokenType idOrkeyword(const char* start, uint32_t length) {
    uint32_t idx = 0;
    while (keywordsToken[idx] != NULL) {
      if (keywordsToken[idx].length == length && \
      memcmp(keywordsToken[idx].keyword, start, length) == 0) {
   return keywordsToken[idx].token;
      }
      idx++;
   }
   return TOKEN_ID;
}

char lookAheadChar(Parser* parser) {
   return *parser->nextCharPtr;
}

static void getNextChar(Parser* parser) {
   parser->curChar = *parser->nextCharPtr++;
}

static bool matchNextChar(Parser* parser, char expectedChar) {
   if (lookAheadChar(parser) == expectedChar) {
      getNextChar(parser);
      return true;
   }
   return false;
}

static void skipBlanks(Parser* parser) {
  while (isspace(parser->curChar)) {
    if (parser->curChar == '\n') {
 parser->curToken.lineNo++;
    }
    getNextChar(parser);
  }
}

static void parseId(Parser* parser, TokenType type) {
  while (isalnum(parser->curChar) || parser->curChar == '_') {
     getNextChar(parser);
  }

  uint32_t length = (uint32_t)(parser->nextCharPtr - parser->curToken.start - 1);
  if (type != TOKEN_UNKNOWN) {
     parser->curToken.type = type;
  } else {
    parser->curToken.type = idOrkeyword(parser->curToken.start, length);
  }
  parser->curToken.length = length;
}

static void parseHexNum(Parser* parser) {
   while (isxdigit(parser->curChar)) {
      getNextChar(parser);
   }
}

static void parseDecNum(Parser* parser) {
  while (isdigit(parser->curChar)) {
     getNextChar(parser);
  }

  if (parser->curChar == '.' && isdigit(lookAheadChar(parser))) {
    getNextChar(parser);
    while (isdigit(parser->curChar)) { //解析小数点之后的数字
 getNextChar(parser);
    }
  }
}

static void parseOctNum(Parser* parser) {
   while(parser->curChar >= '0' && parser->curChar < '8') {
      getNextChar(parser);
   }
}

static void parseNum(Parser* parser) {
  if (parser->curChar == '0' && matchNextChar(parser, 'x')) {
     getNextChar(parser);  //跳过'x'
     parseHexNum(parser);   //解析十六进制数字
     parser->curToken.value =
  NUM_TO_VALUE(strtol(parser->curToken.start, NULL, 16));
} else if (parser->curChar == '0'
&& isdigit(lookAheadChar(parser))) {
  parseOctNum(parser);
  parser->curToken.value =
  NUM_TO_VALUE(strtol(parser->curToken.start, NULL, 8));
} else {
  parseDecNum(parser);
  parser->curToken.value = NUM_TO_VALUE(strtod(parser->curToken.start, NULL));
}
  parser->curToken.length =
    (uint32_t)(parser->nextCharPtr - parser->curToken.start - 1);

  parser->curToken.type = TOKEN_NUM;
}


static void parseUnicodeCodePoint(Parser* parser, ByteBuffer* buf) {
  uint32_t idx = 0;
  int value = 0;
  uint8_t digit = 0;

  while(idx++ < 4) {
    getNextChar(parser);
    if (parser->curChar == '\0') {
 LEX_ERROR(parser, "unterminated unicode!");
    }
    if (parser->curChar >= '0' && parser->curChar <= '9') {
 digit = parser->curChar - '0';
    } else if (parser->curChar >= 'a' && parser->curChar <= 'f') {
 digit = parser->curChar - 'a' + 10;
    } else if (parser->curChar >= 'A' && parser->curChar <= 'F') {
 digit = parser->curChar - 'A' + 10;
    } else {
 LEX_ERROR(parser, "invalid unicode!");
    }
    value = value * 16 | digit;
  }

  uint32_t byteNum = getByteNumOfEncodeUtf8(value);
  ASSERT(byteNum != 0, "utf8 encode bytes should be between 1 and 4!");

  ByteBufferFillWrite(parser->vm, buf, 0, byteNum);
  encodeUtf8(buf->datas + buf->count - byteNum, value);
}

static void parseString(Parser* parser) {
  ByteBuffer str;
  ByteBufferInit(&str);

  while (true) {
    getNextChar(parser);

    if (parser->curChar == '\0') {
 LEX_ERROR(parser, "unterminated string!");
    }

    if (parser->curChar == '"') {
 parser->curToken.type = TOKEN_STRING;
 break;
    }

    if (parser->curChar == '%') {
      if (!matchNextChar(parser, '(')) {
   	    LEX_ERROR(parser, "'%' should followed by '('!");
   	 }
     if (parser->interpolationExpectRightParenNum > 0) {
  	    COMPILE_ERROR(parser, "sorry, I don`t support nest interpolate expression!");
  	 }
     parser->interpolationExpectRightParenNum = 1;
    parser->curToken.type = TOKEN_INTERPOLATION;
    break;
    }

    if (parser->curChar == '\\') {
      getNextChar(parser);
      switch (parser->curChar) {
        case '0':
           ByteBufferAdd(parser->vm, &str, '\0');
           break;
       case 'a':
          ByteBufferAdd(parser->vm, &str, '\a');
          break;
       case 'b':
          ByteBufferAdd(parser->vm, &str, '\b');
          break;
       case 'f':
          ByteBufferAdd(parser->vm, &str, '\f');
          break;
       case 'n':
          ByteBufferAdd(parser->vm, &str, '\n');
          break;
       case 'r':
          ByteBufferAdd(parser->vm, &str, '\r');
          break;
       case 't':
          ByteBufferAdd(parser->vm, &str, '\t');
          break;
      case 'u':
         parseUnicodeCodePoint(parser, &str);
         break;
     case '"':
        ByteBufferAdd(parser->vm, &str, '"');
        break;
     case '\\':
        ByteBufferAdd(parser->vm, &str, '\\');
        break;
    default:
     LEX_ERROR(parser, "unsupport escape \\%c", parser->curChar);
           break;
      }
    } else {
      ByteBufferAdd(parser->vm, &str, parser->curChar);
    }
  }

  ObjString* objString = newObjString(parser->vm, (const char*)str.datas, str.count);
  parser->curToken.value = OBJ_TO_VALUE(objString);
  ByteBufferClear(parser->vm, &str);
}

static void skipAline(Parser* parser) {
    getNextChar(parser);
    while (parser->curChar != '\0') {
      if (parser->curChar == '\n') {
        parser->curToken.lineNo++;
     	 getNextChar(parser);
     	 break;
      }
      getNextChar(parser);
    }
}

static void skipComment(Parser* parser) {
    char nextChar = lookAheadChar(parser);
    if (parser->curChar == '/') {  // 行注释
       skipAline(parser);
    } else {
      while (nextChar != '*' && nextChar != '\0') {
        getNextChar(parser);
        if (parser->curChar == '\n') {
     	    parser->curToken.lineNo++;
     	 }
       nextChar = lookAheadChar(parser);
      }
      if (matchNextChar(parser, '*')) {
        if (!matchNextChar(parser, '/')) {   //匹配*/
     	    LEX_ERROR(parser, "expect '/' after '*'!");
     	 }
       getNextChar(parser);
     } else {
       LEX_ERROR(parser, "expect '*/' before file end!");
     }
    }
    skipBlanks(parser);
}

void getNextToken(Parser* parser) {
  parser->preToken = parser->curToken;
  skipBlanks(parser);
  parser->curToken.type = TOKEN_EOF;
  parser->curToken.length = 0;
  parser->curToken.start = parser->nextCharPtr - 1;
  parser->curToken.value = VT_TO_VALUE(VT_UNDEFINED);
  while (parser->curChar != '\0') {
    switch (parser->curChar) {
      case ',':
   	    parser->curToken.type = TOKEN_COMMA;
   	    break;
      case ':':
   	    parser->curToken.type = TOKEN_COLON;
   	    break;
      case '(':
   	    if (parser->interpolationExpectRightParenNum > 0) {
   	       parser->interpolationExpectRightParenNum++;
   	    }
        parser->curToken.type = TOKEN_LEFT_PAREN;
  	    break;
      case ')':
        if (parser->interpolationExpectRightParenNum > 0) {
          parser->interpolationExpectRightParenNum--;
          if (parser->interpolationExpectRightParenNum == 0) {
     		  parseString(parser);
     		  break;
 	       }
        }
        parser->curToken.type = TOKEN_RIGHT_PAREN;
  	    break;
    case '[':
   	    parser->curToken.type = TOKEN_LEFT_BRACKET;
   	    break;
   	 case ']':
   	    parser->curToken.type = TOKEN_RIGHT_BRACKET;
   	    break;
   	 case '{':
   	    parser->curToken.type = TOKEN_LEFT_BRACE;
   	    break;
   	 case '}':
   	    parser->curToken.type = TOKEN_RIGHT_BRACE;
   	    break;
    case '.':
      if (matchNextChar(parser, '.')) {
         parser->curToken.type = TOKEN_DOT_DOT;
      } else {
         parser->curToken.type = TOKEN_DOT;
      }
      break;
    case '=':
 	    if (matchNextChar(parser, '=')) {
 	       parser->curToken.type = TOKEN_EQUAL;
 	    } else {
 	       parser->curToken.type = TOKEN_ASSIGN;
 	    }
 	    break;
    case '+':
 	    parser->curToken.type = TOKEN_ADD;
 	    break;
 	 case '-':
 	    parser->curToken.type = TOKEN_SUB;
 	    break;
 	 case '*':
 	    parser->curToken.type = TOKEN_MUL;
 	    break;
    case '/':
      if (matchNextChar(parser, '/') || matchNextChar(parser, '*')) {
        skipComment(parser);
        parser->curToken.start = parser->nextCharPtr - 1;
        continue;
      }  else {		 // '/'
	       parser->curToken.type = TOKEN_DIV;
	    }
	    break;
    case '%':
  	    parser->curToken.type = TOKEN_MOD;
  	    break;
    case '&':
        if (matchNextChar(parser, '&')) {
           parser->curToken.type = TOKEN_LOGIC_AND;
        } else {
           parser->curToken.type = TOKEN_BIT_AND;
        }
       break;
   case '|':
	    if (matchNextChar(parser, '|')) {
	       parser->curToken.type = TOKEN_LOGIC_OR;
	    } else {
	       parser->curToken.type = TOKEN_BIT_OR;
	    }
	    break;
   case '~':
 	    parser->curToken.type = TOKEN_BIT_NOT;
 	    break;
 	 case '?':
 	    parser->curToken.type = TOKEN_QUESTION;
 	    break;
  case '>':
       if (matchNextChar(parser, '=')) {
          parser->curToken.type = TOKEN_GREATE_EQUAL;
       } else if (matchNextChar(parser, '>')) {
          parser->curToken.type = TOKEN_BIT_SHIFT_RIGHT;
       } else {
          parser->curToken.type = TOKEN_GREATE;
       }
       break;
    case '<':
       if (matchNextChar(parser, '=')) {
          parser->curToken.type = TOKEN_LESS_EQUAL;
       } else if (matchNextChar(parser, '<')) {
          parser->curToken.type = TOKEN_BIT_SHIFT_LEFT;
       } else {
          parser->curToken.type = TOKEN_LESS;
       }
       break;
    case '!':
       if (matchNextChar(parser, '=')) {
          parser->curToken.type = TOKEN_NOT_EQUAL;
       } else {
          parser->curToken.type = TOKEN_LOGIC_NOT;
       }
       break;

     case '"':
  	    parseString(parser);
  	    break;

    default:
      if (isalpha(parser->curChar) || parser->curChar == '_') {
         parseId(parser, TOKEN_UNKNOWN);  //解析变量名其余的部分
      } else if (isdigit(parser->curChar)) { //数字
	       parseNum(parser);
	    } else {
         if (parser->curChar == '#' && matchNextChar(parser, '!')) {
           skipAline(parser);
           parser->curToken.start = parser->nextCharPtr - 1;
           continue;
        }
        LEX_ERROR(parser, "unsupport char: \'%c\', quit.", parser->curChar);
      }
      return;
    }
    parser->curToken.length = (uint32_t)(parser->nextCharPtr - parser->curToken.start);
    getNextChar(parser);
    return;
  }
}

bool matchToken(Parser* parser, TokenType expected) {
  if (parser->curToken.type == expected) {
     getNextToken(parser);
     return true;
  }
  return false;
}

void consumeCurToken(Parser* parser, TokenType expected, const char* errMsg) {
   if (parser->curToken.type != expected) {
      COMPILE_ERROR(parser, errMsg);
   }
   getNextToken(parser);
}

void consumeNextToken(Parser* parser, TokenType expected, const char* errMsg) {
   getNextToken(parser);
   if (parser->curToken.type != expected) {
      COMPILE_ERROR(parser, errMsg);
   }
}

void initParser(VM* vm, Parser* parser, const char* file, const char* sourceCode, ObjModule* objModule) {
    parser->file = file;
    parser->sourceCode = sourceCode;
    parser->curChar = *parser->sourceCode;
    parser->nextCharPtr = parser->sourceCode + 1;
    parser->curToken.lineNo = 1;
    parser->curToken.type = TOKEN_UNKNOWN;
    parser->curToken.start = NULL;
    parser->curToken.length = 0;
    parser->preToken = parser->curToken;
    parser->interpolationExpectRightParenNum = 0;
    parser->vm = vm;
    parser->curModule = objModule;
}

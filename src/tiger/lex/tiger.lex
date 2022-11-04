%filenames = "scanner"

/*
  * Please don't modify the lines above.
  */

/* You can add lex definitions here. */
/* TODO: Put your lab2 code here */

letter [a-zA-Z]
digits [0-9]+

%x COMMENT STR IGNORE

%%
 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
			*   Parser::ID
			*   Parser::STRING
			*   Parser::INT
			*   Parser::COMMA
			*   Parser::COLON
			*   Parser::SEMICOLON
			*   Parser::LPAREN
			*   Parser::RPAREN
			*   Parser::LBRACK
			*   Parser::RBRACK
			*   Parser::LBRACE
			*   Parser::RBRACE
			*   Parser::DOT
			*   Parser::PLUS
			*   Parser::MINUS
			*   Parser::TIMES
			*   Parser::DIVIDE
			*   Parser::EQ
			*   Parser::NEQ
			*   Parser::LT
			*   Parser::LE
			*   Parser::GT
			*   Parser::GE
			*   Parser::AND
			*   Parser::OR
			*   Parser::ASSIGN
			*   Parser::ARRAY
			*   Parser::IF
			*   Parser::THEN
			*   Parser::ELSE
			*   Parser::WHILE
			*   Parser::FOR
			*   Parser::TO
			*   Parser::DO
			*   Parser::LET
			*   Parser::IN
			*   Parser::END
			*   Parser::OF
			*   Parser::BREAK
			*   Parser::NIL
			*   Parser::FUNCTION
			*   Parser::VAR
			*   Parser::TYPE
  */

 /* reserved words */
"array" { adjust(); return Parser::ARRAY;}
"while" { adjust(); return Parser::WHILE;}
"for" { adjust(); return Parser::FOR;}
"to" { adjust(); return Parser::TO;}
"break" { adjust(); return Parser::BREAK;}
"let" { adjust(); return Parser::LET;}
"in" { adjust(); return Parser::IN;}
"end" { adjust(); return Parser::END;}
"function" { adjust(); return Parser::FUNCTION;}
"var" { adjust(); return Parser::VAR;}
"type" { adjust(); return Parser::TYPE;}
"if" { adjust(); return Parser::IF;}
"then" { adjust(); return Parser::THEN;}
"else" { adjust(); return Parser::ELSE;}
"do" { adjust(); return Parser::DO;}
"of" { adjust(); return Parser::OF;}
"nil" { adjust(); return Parser::NIL;}

 /* Punctuation symbols */
"(" { adjust(); return Parser::LPAREN;}
")" { adjust(); return Parser::RPAREN;}
":" { adjust(); return Parser::COLON;}
"," { adjust(); return Parser::COMMA;}
">" { adjust(); return Parser::GT;}
"<" { adjust(); return Parser::LT;}
">=" { adjust(); return Parser::GE;}
"<=" { adjust(); return Parser::LE;}
"=" { adjust(); return Parser::EQ;}
"<>" { adjust(); return Parser::NEQ;}
":=" { adjust(); return Parser::ASSIGN;}
"&" { adjust(); return Parser::AND;}
"|" { adjust(); return Parser::OR;}
"+" { adjust(); return Parser::PLUS;}
"-" { adjust(); return Parser::MINUS;}
"*" { adjust(); return Parser::TIMES;}
"/" { adjust(); return Parser::DIVIDE;}
"[" { adjust(); return Parser::LBRACK;}
"]" { adjust(); return Parser::RBRACK;}
"{" { adjust(); return Parser::LBRACE;}
"}" { adjust(); return Parser::RBRACE;}
";" { adjust(); return Parser::SEMICOLON;}
"." { adjust(); return Parser::DOT;}

"/*" {
	comment_level_ ++;
	// printf("comment!");
	more();
	begin(StartCondition__::COMMENT);
}
\" {
	strlen_dif_ = 1;
	str_error_ = false;
	begin(StartCondition__::STR);
}

{digits} { adjust(); return Parser::INT;}
{letter}[a-zA-Z0-9_]* { adjust(); return Parser::ID;}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust(); }
\n { adjust(); errormsg_->Newline();}

 /* illegal input */
. { adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}

<COMMENT> {
	"/*" {
		more();
		comment_level_ ++;
	}
	"*/" {
		comment_level_ --;
		if (comment_level_ == 1) {
			adjust();
			begin(StartCondition__::INITIAL);
		} else {
			more();
		}
	}
	\n {more(); errormsg_->Newline();}
	. {more();}
	<<EOF>> {
		/* adjust(); */
		errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");
		begin(StartCondition__::INITIAL);
	}
}

<STR> {
	\" {
		std::string token = matched();
		token.pop_back();
		setMatched(token);

		adjust();
		strlen_dif_ ++;
		char_pos_ += strlen_dif_;

		begin(StartCondition__::INITIAL);
		if (!str_error_) return Parser::STRING;
		else errormsg_->Error(errormsg_->tok_pos_, "illegal string");
	}
	\\n {
		std::string token = matched();
		setMatched(token.substr(0, token.size() - 2) + "\n");

		strlen_dif_ ++;
		more();
	}
	\\t {
		std::string token = matched();
		setMatched(token.substr(0, token.size() - 2) + "\t");

		strlen_dif_ ++;
		more();
	}
	\\\\ {
		std::string token = matched();
		setMatched(token.substr(0, token.size() - 2) + "\\");

		strlen_dif_ ++;
		more();
	}
	\\\" {
		std::string token = matched();
		setMatched(token.substr(0, token.size() - 2) + "\"");

		strlen_dif_ ++;
		more();
	}
	\\([0-9]){3} {
		/* std::cout << "hello!" << std::endl; */
		std::string token = matched();
		std::string numstr = token.substr(token.size() - 3, 3);
		int num = std::stoi(numstr, nullptr, 10);
		if (num > 127) str_error_ = true;
		/* std::cout << matched() << ' ' << numstr << ' ' << num << ' '; */
		setMatched(token.substr(0,token.size()-4) + (char)num);

		strlen_dif_ += 3;
		more();
	}
	\\\^[@-\[] {
		/* std::cout << "hi!" << std::endl; */
		std::string token = matched();
		char ch = token[token.size() - 1];
		int num = ch - '@';
		/* std::cout << token << ' ' << ch << ' ' << num << ' '; */
		setMatched(token.substr(0,token.size()-3) + (char)num);

		strlen_dif_ += 2;
		more();
	} 
	\\/\n |
	\\/\t |
	\\/\f |
	\\/\x20 {
		strlen_dif_ ++;
		setMatched(matched().substr(0, matched().size() - 1));
		more();
		begin(StartCondition__::IGNORE);
	}
	\n {more(); errormsg_->Newline();}
	. {more();}
	<<EOF>> {
		/* adjust(); */
		errormsg_->Error(errormsg_->tok_pos_, "unclosed string");
		begin(StartCondition__::INITIAL);
	}
}

<IGNORE> {
	\\ {
		strlen_dif_ ++;
		setMatched(matched().substr(0, matched().size() - 1));
		more();
		begin(StartCondition__::STR);
	}
	\n {
		errormsg_->Newline();
		strlen_dif_ ++;
		setMatched(matched().substr(0, matched().size() - 1));
		more();
	}
	\t |
	\f |
	\x20 {
		strlen_dif_ ++;
		setMatched(matched().substr(0, matched().size() - 1));
		more();
	}
	. {
		strlen_dif_ ++; 
		setMatched(matched().substr(0, matched().size() - 1));
		more();
		str_error_ = true;
	}
	<<EOF>> {
		/* adjust(); */
		errormsg_->Error(errormsg_->tok_pos_, "unclosed ignore string");
		begin(StartCondition__::INITIAL);
	}
}


package main

import "testing"

func TestLexPreservesSourceAndClassifiesTokens(t *testing.T) {
	source := "void test() {\n  // comment\n  float x = 1.25e-2;\n  char *s = \"a\\\"b\";\n  self->health += 1;\n}"
	got, err := lex(source)
	if err != nil {
		t.Fatalf("lex failed: %v", err)
	}
	var rebuilt string
	for _, tok := range got {
		rebuilt += tok.text
	}
	if rebuilt != source {
		t.Fatalf("lexer did not preserve source\ngot:  %q\nwant: %q", rebuilt, source)
	}
	assertToken := func(text string, kind tokenKind) {
		t.Helper()
		for _, tok := range got {
			if tok.text == text && tok.kind == kind {
				return
			}
		}
		t.Errorf("missing token %q with kind %v", text, kind)
	}
	assertToken("void", tokenIdentifier)
	assertToken("// comment", tokenComment)
	assertToken("1.25e-2", tokenNumber)
	assertToken("\"a\\\"b\"", tokenString)
	assertToken("->", tokenOperator)
	assertToken("+=", tokenOperator)
}

func TestLexReportsUnterminatedLiterals(t *testing.T) {
	for _, source := range []string{"/* no end", "\"no end", "'x"} {
		if _, err := lex(source); err == nil {
			t.Errorf("expected an error for %q", source)
		}
	}
}

func TestLexTracksLineAndColumn(t *testing.T) {
	got, err := lex("a\r\nb")
	if err != nil {
		t.Fatal(err)
	}
	for _, tok := range got {
		if tok.text == "b" && (tok.line != 2 || tok.column != 1) {
			t.Fatalf("b location = %d:%d, want 2:1", tok.line, tok.column)
		}
	}
}

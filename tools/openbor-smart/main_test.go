package main

import "testing"

func TestConvertAnimationTextConvertsLegacyCommands(t *testing.T) {
	in := "anim idle\n\t@cmd changeentityproperty getlocalvar(\"self\") \"velocity\" 0 1 0\n" +
		"@cmd shoot \"cape with space\" 21 13 0\n" +
		"@cmd keyint \"ANI_FREESPECIAL16\" 0 \"UA\" 0 200\n" +
		"@cmd clearL\n"
	want := "anim idle\n\t@cmd changeentityproperty(getlocalvar(\"self\"),\"velocity\",0,1,0)\n" +
		"@cmd shoot(\"cape with space\",21,13,0)\n" +
		"@cmd keyint(\"ANI_FREESPECIAL16\",0,\"UA\",0,200)\n" +
		"@cmd clearL()\n"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != want {
		t.Fatalf("converted animation text mismatch\ngot:  %q\nwant: %q", got, want)
	}
}

func TestConvertAnimationTextPreservesOtherTextAndExistingCalls(t *testing.T) {
	in := "anim idle\r\n  @cmd clearL();  \r\n@cmd\r\n@cmdx untouched\r\n"
	want := "anim idle\r\n  @cmd clearL()  \r\n@cmd\r\n@cmdx untouched\r\n"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != want {
		t.Fatalf("animation text changed unexpectedly\ngot:  %q\nwant: %q", got, want)
	}
}

func TestConvertAnimationTextHandlesTabsAndNoArguments(t *testing.T) {
	in := "@cmd\tclearL\r\n@cmd\tshoot\t\"cape\"\t21\r"
	want := "@cmd clearL()\r\n@cmd shoot(\"cape\",21)\r"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != want {
		t.Fatalf("converted animation text mismatch\ngot:  %q\nwant: %q", got, want)
	}
}

func TestConvertAnimationTextRejectsUnterminatedQuotedArgument(t *testing.T) {
	if _, err := ConvertAnimationText("@cmd shoot \"cape 21"); err == nil {
		t.Fatal("expected an error for an unterminated quoted argument")
	}
}

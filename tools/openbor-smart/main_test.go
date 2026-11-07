package main

import "testing"

func TestConvertCmdLine(t *testing.T) {
	in := "@cmd changeentityproperty getlocalvar(\"self\") \"velocity\" 0 1 0"
	want := "@cmd changeentityproperty(getlocalvar(\"self\"),\"velocity\",0,1,0)"
	got, changed, err := convertCmdLine(in)
	if err != nil || !changed || got != want {
		t.Fatalf("got %q, changed=%v, err=%v; want %q", got, changed, err, want)
	}
}

func TestConvertAnimationText(t *testing.T) {
	in := "anim idle\n\t@cmd clearL\n"
	want := "anim idle\n\t@cmd clearL()\n"
	got, err := ConvertAnimationText(in)
	if err != nil || got != want {
		t.Fatalf("got %q, err=%v; want %q", got, err, want)
	}
}

func TestMalformedCommand(t *testing.T) {
	if _, err := ConvertAnimationText("anim idle\n@cmd\n"); err == nil {
		t.Fatal("expected an error for a command without a function name")
	}
}

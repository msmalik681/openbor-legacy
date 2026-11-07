package main

import "testing"

func TestConvertAnimationTextPreservesLegacyCommands(t *testing.T) {
	in := "anim idle\n\t@cmd changeentityproperty getlocalvar(\"self\") \"velocity\" 0 1 0\n" +
		"@cmd shoot \"cape\" 21 13 0\n" +
		"@cmd keyint \"ANI_FREESPECIAL16\" 0 \"UA\" 0 200\n"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != in {
		t.Fatalf("animation text changed\ngot:  %q\nwant: %q", got, in)
	}
}

func TestConvertAnimationTextPreservesCRLFAndSpacing(t *testing.T) {
	in := "anim idle\r\n    @cmd  clearL   \r\n@cmd\r\n"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != in {
		t.Fatalf("animation text changed\ngot:  %q\nwant: %q", got, in)
	}
}

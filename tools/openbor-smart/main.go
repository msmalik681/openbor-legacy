package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// ConvertAnimationText currently preserves animation text verbatim.
// Legacy @cmd syntax is interpreted by the OpenBOR Lua engine at load time;
// game files must not be rewritten by this tool.
func ConvertAnimationText(input string) (string, error) {
	return input, nil
}

func outputPathForInput(inputPath string) string {
	ext := filepath.Ext(inputPath)
	if ext == "" {
		return inputPath + ".lua"
	}
	return strings.TrimSuffix(inputPath, ext) + ".lua"
}

func run() error {
	inputPath := flag.String("in", "", "input script or animation file")
	flag.Parse()
	if *inputPath == "" {
		return fmt.Errorf("usage: openbor-smart -in input.c")
	}
	outputPath := outputPathForInput(*inputPath)
	data, err := os.ReadFile(*inputPath)
	if err != nil {
		return fmt.Errorf("read input: %w", err)
	}
	converted, err := ConvertAnimationText(string(data))
	if err != nil {
		return fmt.Errorf("convert input: %w", err)
	}
	if err := os.WriteFile(outputPath, []byte(converted), 0o644); err != nil {
		return fmt.Errorf("write output %q: %w", outputPath, err)
	}
	return nil
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

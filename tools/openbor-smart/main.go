package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
)

// ConvertAnimationText currently preserves animation text verbatim.
//
// Legacy OpenBOR already accepts whitespace-separated @cmd arguments. Rewriting
// those commands as parenthesized Lua calls would change valid source text and
// can break expressions containing spaces, quotes, or nested calls. Animation
// script blocks will be translated by a separate, syntax-aware conversion pass.
func ConvertAnimationText(input string) (string, error) {
	return input, nil
}

func run() error {
	inputPath := flag.String("in", "", "input animation text file")
	outputPath := flag.String("out", "", "output file")
	flag.Parse()
	if *inputPath == "" || *outputPath == "" {
		return fmt.Errorf("usage: openbor-smart -in input.txt -out output.txt")
	}
	data, err := os.ReadFile(*inputPath)
	if err != nil {
		return fmt.Errorf("read input: %w", err)
	}
	converted, err := ConvertAnimationText(string(data))
	if err != nil {
		return fmt.Errorf("convert input: %w", err)
	}
	if dir := filepath.Dir(*outputPath); dir != "." {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return fmt.Errorf("create output directory: %w", err)
		}
	}
	if err := os.WriteFile(*outputPath, []byte(converted), 0o644); err != nil {
		return fmt.Errorf("write output: %w", err)
	}
	return nil
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

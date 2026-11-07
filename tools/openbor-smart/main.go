package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func convertCmdLine(line string) (string, bool, error) {
	trimmed := strings.TrimLeft(line, " \t\r")
	indent := line[:len(line)-len(trimmed)]
	fields := strings.Fields(trimmed)
	if len(fields) == 0 || !strings.EqualFold(fields[0], "@cmd") {
		return line, false, nil
	}
	if len(fields) < 2 {
		return "", true, fmt.Errorf("@cmd is missing a function name")
	}
	return indent + "@cmd " + fields[1] + "(" + strings.Join(fields[2:], ",") + ")", true, nil
}

func ConvertAnimationText(input string) (string, error) {
	lines := strings.Split(input, "\n")
	for i, line := range lines {
		cr := strings.HasSuffix(line, "\r")
		body := strings.TrimSuffix(line, "\r")
		converted, changed, err := convertCmdLine(body)
		if err != nil {
			return "", fmt.Errorf("line %d: %w", i+1, err)
		}
		if changed {
			if cr {
				converted += "\r"
			}
			lines[i] = converted
		}
	}
	return strings.Join(lines, "\n"), nil
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

package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// ConvertAnimationText converts legacy whitespace-separated @cmd commands to
// explicit Lua calls. All non-command animation text is preserved.
func ConvertAnimationText(input string) (string, error) {
	var out strings.Builder
	for start := 0; start < len(input); {
		end := start
		for end < len(input) && input[end] != '\n' && input[end] != '\r' {
			end++
		}
		converted, err := convertCommandLine(input[start:end])
		if err != nil {
			return "", err
		}
		out.WriteString(converted)
		if end < len(input) {
			if input[end] == '\r' && end+1 < len(input) && input[end+1] == '\n' {
				out.WriteString("\r\n")
				start = end + 2
			} else {
				out.WriteByte(input[end])
				start = end + 1
			}
		} else {
			start = end
		}
	}
	return out.String(), nil
}

func convertCommandLine(line string) (string, error) {
	i := 0
	for i < len(line) && (line[i] == ' ' || line[i] == '\t') {
		i++
	}
	const directive = "@cmd"
	if !strings.HasPrefix(line[i:], directive) {
		return line, nil
	}
	afterDirective := i + len(directive)
	if afterDirective < len(line) && line[afterDirective] != ' ' && line[afterDirective] != '\t' {
		return line, nil
	}
	indent := line[:i]
	rest := strings.TrimSpace(line[afterDirective:])
	if rest == "" {
		return line, nil
	}
	commandEnd := 0
	for commandEnd < len(rest) && rest[commandEnd] != ' ' && rest[commandEnd] != '\t' {
		commandEnd++
	}
	command := rest[:commandEnd]
	// Preserve calls that are already written in Lua syntax.
	if strings.Contains(command, "(") {
		return line, nil
	}
	if !isLuaIdentifier(command) {
		return "", fmt.Errorf("@cmd: invalid command name %q", command)
	}
	if commandEnd == len(rest) {
		return indent + directive + " " + command + "()", nil
	}
	args, err := splitLegacyCommandArgs(rest[commandEnd:])
	if err != nil {
		return "", err
	}
	var result strings.Builder
	result.WriteString(indent)
	result.WriteString(directive)
	result.WriteByte(' ')
	result.WriteString(command)
	result.WriteByte('(')
	for n, arg := range args {
		if n > 0 {
			result.WriteByte(',')
		}
		result.WriteString(arg)
	}
	result.WriteByte(')')
	return result.String(), nil
}

func splitLegacyCommandArgs(source string) ([]string, error) {
	var args []string
	for i := 0; i < len(source); {
		for i < len(source) && (source[i] == ' ' || source[i] == '\t') {
			i++
		}
		if i == len(source) {
			break
		}
		start := i
		quote := byte(0)
		for i < len(source) {
			c := source[i]
			if quote != 0 {
				if c == '\\' {
					i++
					if i < len(source) {
						i++
					}
					continue
				}
				if c == quote {
					quote = 0
				}
				i++
				continue
			}
			if c == '"' || c == '\'' {
				quote = c
				i++
				continue
			}
			if c == ' ' || c == '\t' {
				break
			}
			i++
		}
		if quote != 0 {
			return nil, fmt.Errorf("@cmd: unterminated quoted argument")
		}
		args = append(args, source[start:i])
	}
	return args, nil
}

func isLuaIdentifier(value string) bool {
	if value == "" || !isIdentifierStart(value[0]) {
		return false
	}
	for i := 1; i < len(value); i++ {
		if !isIdentifierPart(value[i]) {
			return false
		}
	}
	return true
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

package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"path"

	"github.com/gomarkdown/markdown"
)

func fileExists(file string) bool {
	if _, err := os.Stat(file); err == nil {
		return true
	} /* else if errors.Is(err, os.ErrNotExist) {
		return false
	}*/
	// file may not exist (commented out check)
	// or undefined
	return false
}

func printHelp() {
	fmt.Println("USAGE: " + os.Args[0] + " [markdown file]")
	fmt.Println("The name of the output file will be the same with '.html' appended.")
}

// see: https://github.com/gomarkdown/mdtohtml/blob/master/main.go
func main() {
	if len(os.Args) < 2 {
		printHelp()
		os.Exit(1)
	}
	file := os.Args[1]
	if !fileExists(file) {
		log.Fatal(fmt.Sprintf("file '%s' doesn't exist!", file))
	}
	md, err := os.ReadFile(file)
	if err != nil {
		log.Fatal("failed to read file!")
	}
	html := markdown.ToHTML(md, nil, nil)
	var outfilename string
	if len(os.Args) == 3 {
		outfilename = os.Args[2]
	} else {
		_, fn := path.Split(file)
		outfilename = fn + ".html"
	}
	outfile, err := os.OpenFile(outfilename, os.O_WRONLY|os.O_CREATE, 0600)
	if err != nil {
		log.Fatal("failed to open output file!")
	}
	defer outfile.Close()

	wr := bufio.NewWriter(outfile)
	fmt.Fprint(wr, string(html))
	wr.Flush()
}

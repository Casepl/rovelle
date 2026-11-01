package controlplane

import (
	"encoding/json"
	"errors"
	"io"
	"os"
)

var ErrEmptyJournal = errors.New("journal must not be empty")

func AppendRevisionPromoted(path string, event RevisionPromoted) error {
	file, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0600)

	if err != nil {
		return err
	}

	encodeErr := json.NewEncoder(file).Encode(event)
	closeErr := file.Close()

	if encodeErr != nil {
		return encodeErr
	}

	return closeErr
}

func ReplayJournal(path string) (Environment, error) {
	environment := Environment{}
	file, openFileError := os.Open(path)

	if openFileError != nil {
		return environment, openFileError
	}

	defer file.Close()

	decoder := json.NewDecoder(file)
	seenEvent := false

	for {
		var event RevisionPromoted
		decoderError := decoder.Decode(&event)

		if errors.Is(decoderError, io.EOF) {
			if !seenEvent {
				return environment, ErrEmptyJournal
			} else {
				return environment, nil
			}
		}

		if decoderError != nil {
			return environment, decoderError
		}

		if !seenEvent {
			environment.Application = event.Application
			environment.Name = event.Environment
			seenEvent = true
		}

		setDesiredRevisionError := environment.SetDesiredRevision(event.Revision)

		if setDesiredRevisionError != nil {
			return environment, setDesiredRevisionError
		}
	}
}

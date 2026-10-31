package controlplane

import (
	"encoding/json"
	"os"
)

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

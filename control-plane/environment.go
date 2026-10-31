package controlplane

import "errors"

var ErrEmptyRevision = errors.New("revision must not be empty")

type Environment struct {
	Application     string
	Name            string
	DesiredRevision string
}

func (e *Environment) SetDesiredRevision(revision string) error {
	if revision == "" {
		return ErrEmptyRevision
	}

	e.DesiredRevision = revision

	return nil
}

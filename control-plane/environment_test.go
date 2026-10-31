package controlplane

import (
	"errors"
	"testing"
)

func TestSetDesiredRevision(t *testing.T) {
	tests := []struct {
		name            string
		initialRevision string
		newRevision     string
		wantRevision    string
		wantErr         error
	}{
		{
			name:         "assign revision",
			newRevision:  "revision-a",
			wantRevision: "revision-a",
		},
		{
			name:            "replace revision",
			initialRevision: "revision-a",
			newRevision:     "revision-b",
			wantRevision:    "revision-b",
		},
		{
			name:            "reject empty revision",
			initialRevision: "revision-a",
			wantRevision:    "revision-a",
			wantErr:         ErrEmptyRevision,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			environment := Environment{
				Application:     "shop",
				Name:            "production",
				DesiredRevision: test.initialRevision,
			}

			err := environment.SetDesiredRevision(test.newRevision)

			if !errors.Is(err, test.wantErr) {
				t.Fatalf("error = %v, want %v", err, test.wantErr)
			}

			if environment.DesiredRevision != test.wantRevision {
				t.Fatalf(
					"desired revision = %q, want %q",
					environment.DesiredRevision,
					test.wantRevision,
				)
			}
		})
	}
}

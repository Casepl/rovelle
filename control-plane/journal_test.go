package controlplane

import (
	"os"
	"path/filepath"
	"testing"
)

func TestAppendRevisionPromoted(t *testing.T) {
	tests := []struct {
		name   string
		events []RevisionPromoted
		want   string
	}{
		{
			name: "write first event",
			events: []RevisionPromoted{
				NewRevisionPromoted("shop", "production", "revision-a"),
			},
			want: "{\"type\":\"revision_promoted\",\"application\":\"shop\",\"environment\":\"production\",\"revision\":\"revision-a\"}\n",
		},
		{
			name: "append without replacing first event",
			events: []RevisionPromoted{
				NewRevisionPromoted("shop", "production", "revision-a"),
				NewRevisionPromoted("shop", "production", "revision-b"),
			},
			want: "{\"type\":\"revision_promoted\",\"application\":\"shop\",\"environment\":\"production\",\"revision\":\"revision-a\"}\n" +
				"{\"type\":\"revision_promoted\",\"application\":\"shop\",\"environment\":\"production\",\"revision\":\"revision-b\"}\n",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			path := filepath.Join(t.TempDir(), "journal.jsonl")

			for _, event := range test.events {
				if err := AppendRevisionPromoted(path, event); err != nil {
					t.Fatalf("append event: %v", err)
				}
			}

			contents, err := os.ReadFile(path)
			if err != nil {
				t.Fatalf("read journal: %v", err)
			}

			if string(contents) != test.want {
				t.Fatalf("journal = %q, want %q", contents, test.want)
			}
		})
	}
}

func TestReplayJournal(t *testing.T) {
	tests := []struct {
		name   string
		events []RevisionPromoted
		want   Environment
	}{
		{
			name: "append without replacing first event",
			events: []RevisionPromoted{
				NewRevisionPromoted("shop", "production", "revision-a"),
				NewRevisionPromoted("shop", "production", "revision-b"),
			},
			want: Environment{
				Application:     "shop",
				Name:            "production",
				DesiredRevision: "revision-b",
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			path := filepath.Join(t.TempDir(), "journal.jsonl")
			for _, event := range test.events {
				if err := AppendRevisionPromoted(path, event); err != nil {
					t.Fatalf("append event: %v", err)
				}
			}
			environment, error := ReplayJournal(path)

			if error != nil {
				t.Fatalf("replay journal some wrong: %v", error)
			}

			if environment.Application != test.want.Application {
				t.Fatalf(
					"Application = %q, want %q",
					environment.Application,
					test.want.Application,
				)
			}

			if environment.Name != test.want.Name {
				t.Fatalf(
					"name = %q, want %q",
					environment.Name,
					test.want.Name,
				)
			}

			if environment.DesiredRevision != test.want.DesiredRevision {
				t.Fatalf(
					"desired revision = %q, want %q",
					environment.DesiredRevision,
					test.want.DesiredRevision,
				)
			}
		})
	}

}

func TestReplayJournalRejectsMalformedJSON(t *testing.T) {
	tests := []struct {
		name string
		rows []string
	}{
		{
			name: "reject malformed second event",
			rows: []string{
				"{\"type\":\"revision_promoted\",\"application\":\"shop\",\"environment\":\"production\",\"revision\":\"revision-a\"}\n",
				"{\"type\":\"revision_promoted\",\"application\":\"shop\",\"environment\":\"production\",\n",
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			path := filepath.Join(t.TempDir(), "journal.jsonl")
			contents := ""

			for _, row := range test.rows {
				contents += row
			}

			if err := os.WriteFile(path, []byte(contents), 0600); err != nil {
				t.Fatalf("write file: %v", err)
			}

			_, err := ReplayJournal(path)

			if err == nil {
				t.Fatal("replay malformed journal: expected error")
			}
		})
	}
}

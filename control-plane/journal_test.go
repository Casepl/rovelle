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

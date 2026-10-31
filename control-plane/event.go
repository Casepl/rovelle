package controlplane

const RevisionPromotedType = "revision_promoted"

type RevisionPromoted struct {
	Type        string `json:"type"`
	Application string `json:"application"`
	Environment string `json:"environment"`
	Revision    string `json:"revision"`
}

func NewRevisionPromoted(
	application string,
	environment string,
	revision string,
) RevisionPromoted {
	return RevisionPromoted{
		Type:        RevisionPromotedType,
		Application: application,
		Environment: environment,
		Revision:    revision,
	}
}

/*
Copyright 2022 The Crossplane Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package v1alpha1

import (
	"reflect"

	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime/schema"

	xpv1 "github.com/crossplane/crossplane-runtime/apis/common/v1"
)

// CalculatorParameters are the configurable fields of a Calculator.
type CalculatorParameters struct {
	ConfigurableField string `json:"configurableField"`
}

// CalculatorObservation are the observable fields of a Calculator.
type CalculatorObservation struct {
	ObservableField string `json:"observableField,omitempty"`
	Precision       string `json:"precision,omitempty"`
	Recall          string `json:"recall,omitempty"`
	F1Score         string `json:"f1Score,omitempty"`
	Accuracy        string `json:"accuracy,omitempty"`
	ErrorRate       string `json:"errorRate,omitempty"`

	// usati per calcolare le metriche
	FalsePositive int `json:"falsePositive,omitempty"`
	FalseNegative int `json:"falseNegative,omitempty"`
	TruePositive  int `json:"truePositive,omitempty"`
	TrueNegative  int `json:"trueNegative,omitempty"`
        Total  	      int `json:"total,omitempty"`
}

// A CalculatorSpec defines the desired state of a Calculator.
type CalculatorSpec struct {
	xpv1.ResourceSpec `json:",inline"`
	ForProvider       CalculatorParameters `json:"forProvider"`
	// ModelPath         string               `json:"modelPath"`
	ServicePort int `json:"servicePort"`
}

// A CalculatorStatus represents the observed state of a Calculator.
type CalculatorStatus struct {
	xpv1.ResourceStatus `json:",inline"`
	AtProvider          CalculatorObservation `json:"atProvider,omitempty"`
}

// +kubebuilder:object:root=true

// A Calculator is an example API type.
// +kubebuilder:printcolumn:name="READY",type="string",JSONPath=".status.conditions[?(@.type=='Ready')].status"
// +kubebuilder:printcolumn:name="SYNCED",type="string",JSONPath=".status.conditions[?(@.type=='Synced')].status"
// +kubebuilder:printcolumn:name="EXTERNAL-NAME",type="string",JSONPath=".metadata.annotations.crossplane\\.io/external-name"
// +kubebuilder:printcolumn:name="AGE",type="date",JSONPath=".metadata.creationTimestamp"
// +kubebuilder:printcolumn:name="PRECISION",type="string",JSONPath=".status.atProvider.precision"
// +kubebuilder:printcolumn:name="RECALL",type="string",JSONPath=".status.atProvider.recall"
// +kubebuilder:printcolumn:name="F1SCORE",type="string",JSONPath=".status.atProvider.f1Score"
// +kubebuilder:printcolumn:name="ACCURACY",type="string",JSONPath=".status.atProvider.accuracy"
// +kubebuilder:printcolumn:name="ERRORRATE",type="string",JSONPath=".status.atProvider.errorRate"
// +kubebuilder:subresource:status
// +kubebuilder:resource:scope=Cluster,categories={crossplane,managed,metricsprovider}
type Calculator struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   CalculatorSpec   `json:"spec"`
	Status CalculatorStatus `json:"status,omitempty"`
}

// +kubebuilder:object:root=true

// CalculatorList contains a list of Calculator
type CalculatorList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []Calculator `json:"items"`
}

// Calculator type metadata.
var (
	CalculatorKind             = reflect.TypeOf(Calculator{}).Name()
	CalculatorGroupKind        = schema.GroupKind{Group: Group, Kind: CalculatorKind}.String()
	CalculatorKindAPIVersion   = CalculatorKind + "." + SchemeGroupVersion.String()
	CalculatorGroupVersionKind = SchemeGroupVersion.WithKind(CalculatorKind)
)

func init() {
	SchemeBuilder.Register(&Calculator{}, &CalculatorList{})
}

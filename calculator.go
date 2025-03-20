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

package calculator

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/pkg/errors"
	"k8s.io/apimachinery/pkg/types"

	"github.com/crossplane/crossplane-runtime/pkg/connection"
	"github.com/crossplane/crossplane-runtime/pkg/controller"
	"github.com/crossplane/crossplane-runtime/pkg/event"
	"github.com/crossplane/crossplane-runtime/pkg/ratelimiter"
	"github.com/crossplane/crossplane-runtime/pkg/reconciler/managed"
	"github.com/crossplane/crossplane-runtime/pkg/resource"
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"github.com/crossplane/provider-metricsprovider/apis/metrics/v1alpha1"
	apisv1alpha1 "github.com/crossplane/provider-metricsprovider/apis/v1alpha1"
	"github.com/crossplane/provider-metricsprovider/internal/features"

	// Kubernetes specific imports
	appsv1 "k8s.io/api/apps/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/intstr"
	"k8s.io/utils/pointer"
)

const (
	errNotCalculator = "managed resource is not a Calculator custom resource"
	errTrackPCUsage  = "cannot track ProviderConfig usage"
	errGetPC         = "cannot get ProviderConfig"
	errGetCreds      = "cannot get credentials"
	errNewClient     = "cannot create new Service"

	defaultPort    = 30080
	nodePort       = 30080
	deploymentYAML = "/var/data/deployment-server.yaml"
	serviceYAML    = "/var/data/service-server.yaml"
	OUTPUT_SIZE    = 10
)

var z int = 0

type Metrics struct {
	Precision string `json:"precision"`
	Recall    string `json:"recall"`
	F1Score   string `json:"f1_score"`
	Accuracy  string `json:"accuracy"`
	ErrorRate string `json:"error_rate"`
}

// A NoOpService does nothing.
type NoOpService struct{}

var (
	newNoOpService = func(_ []byte) (interface{}, error) { return &NoOpService{}, nil }
)

// Setup adds a controller that reconciles Calculator managed resources.
func Setup(mgr ctrl.Manager, o controller.Options) error {
	name := managed.ControllerName(v1alpha1.CalculatorGroupKind)

	cps := []managed.ConnectionPublisher{managed.NewAPISecretPublisher(mgr.GetClient(), mgr.GetScheme())}
	if o.Features.Enabled(features.EnableAlphaExternalSecretStores) {
		cps = append(cps, connection.NewDetailsManager(mgr.GetClient(), apisv1alpha1.StoreConfigGroupVersionKind))
	}

	r := managed.NewReconciler(mgr,
		resource.ManagedKind(v1alpha1.CalculatorGroupVersionKind),
		managed.WithExternalConnecter(&connector{
			kube:         mgr.GetClient(),
			usage:        resource.NewProviderConfigUsageTracker(mgr.GetClient(), &apisv1alpha1.ProviderConfigUsage{}),
			newServiceFn: newNoOpService}),
		managed.WithLogger(o.Logger.WithValues("controller", name)),
		managed.WithPollInterval(o.PollInterval),
		managed.WithRecorder(event.NewAPIRecorder(mgr.GetEventRecorderFor(name))),
		managed.WithConnectionPublishers(cps...))

	return ctrl.NewControllerManagedBy(mgr).
		Named(name).
		WithOptions(o.ForControllerRuntime()).
		WithEventFilter(resource.DesiredStateChanged()).
		For(&v1alpha1.Calculator{}).
		Complete(ratelimiter.NewReconciler(name, r, o.GlobalRateLimiter))
}

// A connector is expected to produce an ExternalClient when its Connect method
// is called.
type connector struct {
	kube         client.Client
	usage        resource.Tracker
	newServiceFn func(creds []byte) (interface{}, error)
}

// Connect typically produces an ExternalClient by:
// 1. Tracking that the managed resource is using a ProviderConfig.
// 2. Getting the managed resource's ProviderConfig.
// 3. Getting the credentials specified by the ProviderConfig.
// 4. Using the credentials to form a client.
func (c *connector) Connect(ctx context.Context, mg resource.Managed) (managed.ExternalClient, error) {
	cr, ok := mg.(*v1alpha1.Calculator)
	if !ok {
		return nil, errors.New(errNotCalculator)
	}

	if err := c.usage.Track(ctx, mg); err != nil {
		return nil, errors.Wrap(err, errTrackPCUsage)
	}

	pc := &apisv1alpha1.ProviderConfig{}
	if err := c.kube.Get(ctx, types.NamespacedName{Name: cr.GetProviderConfigReference().Name}, pc); err != nil {
		return nil, errors.Wrap(err, errGetPC)
	}

	cd := pc.Spec.Credentials
	data, err := resource.CommonCredentialExtractor(ctx, cd.Source, c.kube, cd.CommonCredentialSelectors)
	if err != nil {
		return nil, errors.Wrap(err, errGetCreds)
	}

	svc, err := c.newServiceFn(data)
	if err != nil {
		return nil, errors.Wrap(err, errNewClient)
	}

	return &external{service: svc, client: c.kube}, nil
}

// An ExternalClient observes, then either creates, updates, or deletes an
// external resource to ensure it reflects the managed resource's desired state.
type external struct {
	// A 'client' used to connect to the external resource API. In practice this
	// would be something like an AWS SDK client.
	service interface{}
	client  client.Client
	// kube    kubernetes.Interface
}

var create int = 0

func (c *external) Observe(ctx context.Context, mg resource.Managed) (managed.ExternalObservation, error) {
	cr, ok := mg.(*v1alpha1.Calculator)
	if !ok {
		return managed.ExternalObservation{}, errors.New(errNotCalculator)
	}

	var deployment appsv1.Deployment

	if c.client == nil {
		return managed.ExternalObservation{}, errors.New("client is nil")
	}

	log := ctrl.LoggerFrom(ctx)
	log.Info("Getting deployment", "name", "deployment-mnist", "namespace", "crossplane-system")
	fmt.Printf("\n")
	err := c.client.Get(ctx, types.NamespacedName{Name: "deployment-mnist", Namespace: "crossplane-system"}, &deployment)
	if err != nil {
		if apierrors.IsNotFound(err) && create == 0 {
			// La risorsa non esiste oppure Create è 0, quindi chiama Create
			if _, err := c.Create(ctx, mg); err != nil {
				return managed.ExternalObservation{}, errors.Wrap(err, "failed to create deployment\n")
			} else {
				create = 1
			}
		}
	}

	// These fmt statements should be removed in the real implementation.
	fmt.Printf("Observing: %+v\n", cr)

	tp, fp, fn, tn, total, result, err := processFile()
	if err != nil {
		return managed.ExternalObservation{}, errors.Wrap(err, "failed to process file\n")
	}

	if result == 1 {
		fmt.Printf("Si calcolano le metriche\n")
		tp += cr.Status.AtProvider.TruePositive
		fp += cr.Status.AtProvider.FalsePositive
		fn += cr.Status.AtProvider.FalseNegative
		tn += cr.Status.AtProvider.TrueNegative
		total += cr.Status.AtProvider.Total

		metrics, res := calculateMetrics(tp, fp, fn, tn, total)

		if res == 1 {
			cr.Status.AtProvider.Precision = metrics.Precision
			cr.Status.AtProvider.Recall = metrics.Recall
			cr.Status.AtProvider.F1Score = metrics.F1Score
			cr.Status.AtProvider.Accuracy = metrics.Accuracy
			cr.Status.AtProvider.ErrorRate = metrics.ErrorRate

			cr.Status.AtProvider.TruePositive = tp
			cr.Status.AtProvider.FalsePositive = fp
			cr.Status.AtProvider.FalseNegative = fn
			cr.Status.AtProvider.TrueNegative = tn
			cr.Status.AtProvider.Total = total

			// Usa il logger di Crossplane
			log := ctrl.LoggerFrom(ctx)
			log.Info("Calculated Metrics: ", "Precision", cr.Status.AtProvider.Precision, "Recall", cr.Status.AtProvider.Recall, "F1Score",cr.Status.AtProvider.F1Score,
				 "Accuracy", cr.Status.AtProvider.Accuracy, "ErrorRate", cr.Status.AtProvider.ErrorRate, "TP", cr.Status.AtProvider.TruePositive,
				 "FP", cr.Status.AtProvider.FalsePositive, "FN", cr.Status.AtProvider.FalseNegative, "TN", cr.Status.AtProvider.TrueNegative, "Total", cr.Status.AtProvider.Total)
			fmt.Printf("\n")

			checkMetrics(metrics)
		}
	}

	return managed.ExternalObservation{
		// Return false when the external resource does not exist. This lets
		// the managed resource reconciler know that it needs to call Create to
		// (re)create the resource, or that it has successfully been deleted.
		ResourceExists: true,

		// Return false when the external resource exists, but it not up to date
		// with the desired managed resource state. This lets the managed
		// resource reconciler know that it needs to call Update.
		ResourceUpToDate: true,

		// Return any details that may be required to connect to the external
		// resource. These will be stored as the connection secret.
		ConnectionDetails: managed.ConnectionDetails{},
	}, nil
}

func (c *external) Create(ctx context.Context, mg resource.Managed) (managed.ExternalCreation, error) {
	cr, ok := mg.(*v1alpha1.Calculator)
	if !ok {
		return managed.ExternalCreation{}, errors.New(errNotCalculator)
	}

	fmt.Printf("Creating: %+v\n", cr)

	// Creazione Deployment
	deployment := &appsv1.Deployment{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "deployment-mnist",
			Namespace: "crossplane-system",
			Labels: map[string]string{
				"app": "deployment-mnist",
			},
		},
		Spec: appsv1.DeploymentSpec{
			Replicas: pointer.Int32(1),
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					"app": "deployment-mnist",
				},
			},
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						"app": "deployment-mnist",
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:            "ml-worker",
							Image:           "filippogiorgi4/mnist:1.1",
							ImagePullPolicy: corev1.PullAlways,
							Command:         []string{"/usr/src/app/server"},
							Args:            []string{"/usr/src/app/model_mnist_small.tflite"},
							Ports: []corev1.ContainerPort{
								{ContainerPort: 30080},
							},
							VolumeMounts: []corev1.VolumeMount{
								{
									MountPath: "/var/data/",
									Name:      "data-volume",
								},
							},
							Env: []corev1.EnvVar{
								{Name: "FOLDER_PATH", Value: "/var/data/"},
								{Name: "OUTPUT_PATH", Value: "ml_model_prova"},
								{Name: "DATA_PATH", Value: "data.csv"},
								{Name: "LOGGING_LEVEL", Value: "INFO"},
								{Name: "LD_LIBRARY_PATH", Value: "/home/fgiorgi/.local/lib:$LD_LIBRARY_PATH"},
							},
						},
					},
					RestartPolicy: corev1.RestartPolicyAlways,
					Volumes: []corev1.Volume{
						{
							Name: "data-volume",
							VolumeSource: corev1.VolumeSource{
								PersistentVolumeClaim: &corev1.PersistentVolumeClaimVolumeSource{
									ClaimName: "data-pvc",
								},
							},
						},
					},
				},
			},
		},
	}

	// Creazione Service
	service := &corev1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "deployment-mnist-service",
			Namespace: "crossplane-system",
		},
		Spec: corev1.ServiceSpec{
			Selector: map[string]string{
				"app": "deployment-mnist",
			},
			Ports: []corev1.ServicePort{
				{
					Protocol:   corev1.ProtocolTCP,
					Port:       30080,
					TargetPort: intstr.FromInt(30080),
					NodePort:   30080,
				},
			},
			Type: corev1.ServiceTypeLoadBalancer,
		},
	}

	// Creazione delle risorse nel cluster
	if err := c.client.Create(ctx, deployment); err != nil {
		return managed.ExternalCreation{}, errors.Wrap(err, "failed to create deployment\n")
	}
	if err := c.client.Create(ctx, service); err != nil {
		return managed.ExternalCreation{}, errors.Wrap(err, "failed to create service\n")
	}

	// Log delle metriche inizializzate
	log := ctrl.LoggerFrom(ctx)
	log.Info("Created Deployment and Service", "namespace", deployment.Namespace, "deployment", deployment.Name, "service", service.Name)
	fmt.Printf("\n")
	return managed.ExternalCreation{
		ConnectionDetails: managed.ConnectionDetails{},
	}, nil
}

func (c *external) Update(ctx context.Context, mg resource.Managed) (managed.ExternalUpdate, error) {
	cr, ok := mg.(*v1alpha1.Calculator)
	if !ok {
		return managed.ExternalUpdate{}, errors.New(errNotCalculator)
	}

	fmt.Printf("Updating: %+v\n", cr)

	return managed.ExternalUpdate{
		// Optionally return any details that may be required to connect to the
		// external resource. These will be stored as the connection secret.
		ConnectionDetails: managed.ConnectionDetails{},
	}, nil
}

func (c *external) Delete(ctx context.Context, mg resource.Managed) error {
	cr, ok := mg.(*v1alpha1.Calculator)
	if !ok {
		return errors.New(errNotCalculator)
	}

	fmt.Printf("Deleting: %+v\n", cr)

	// Step 1: Delete the Deployment (if needed)
	deployment := &appsv1.Deployment{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "deployment-mnist",  // Nome del deployment dal spec
			Namespace: "crossplane-system", // Namespace del calculator
		},
	}
	err := c.client.Get(ctx, client.ObjectKey{Name: "deployment-mnist", Namespace: "crossplane-system"}, deployment)
	if err != nil && !apierrors.IsNotFound(err) {
		return errors.Wrap(err, "failed to get deployment\n")
	}
	if err == nil {
		err = c.client.Delete(ctx, deployment)
		if err != nil {
			return errors.Wrap(err, "failed to delete deployment\n")
		}
	}

	// Step 2: Delete the Service (if needed)
	service := &corev1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "deployment-mnist-service", // Nome del service dal spec
			Namespace: "crossplane-system",        // Namespace del calculator
		},
	}
	err = c.client.Get(ctx, client.ObjectKey{Name: "deployment-mnist-service", Namespace: "crossplane-system"}, service)
	if err != nil && !apierrors.IsNotFound(err) {
		return errors.Wrap(err, "failed to get service\n")
	}
	if err == nil {
		err = c.client.Delete(ctx, service)
		if err != nil {
			return errors.Wrap(err, "failed to delete service\n")
		}
	}

	// Step 3: Remove finalizer from Calculator if needed
	if len(cr.GetFinalizers()) > 0 {
		cr.SetFinalizers([]string{})
		err = c.client.Update(ctx, cr)
		if err != nil {
			return errors.Wrap(err, "failed to remove finalizer from calculator\n")
		}
	}

	return nil
}

var lastReadPosition int64 = 0
var newmetrics int = 0
var res int = 0

func processFile() (int, int, int, int, int, int, error) {
	newmetrics = 0
	res = 0
	fmt.Printf("ProcessFile\n")
	file, err := os.Open("/var/data/ml_model_prova/labels/y_test.csv")
	if err != nil {
		return z, z, z, z, z, z, errors.Wrap(err, "cannot open CSV file\n")
	}
	defer func() {
		if cerr := file.Close(); cerr != nil {
			fmt.Printf("Error closing file: %v\n", cerr)
		}
	}()

	// Usa Seek per andare alla posizione dell'ultimo indice letto
	_, err = file.Seek(lastReadPosition, 0)
	if err != nil {
		return z, z, z, z, z, z, errors.Wrap(err, "failed to seek file\n")
	}

	var predictions []int
	scanner := bufio.NewScanner(file)

	// Leggi il file riga per riga
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		} else {

			value, err := strconv.Atoi(line)
			// fmt.Printf("%d\n", value)
			if err != nil {
				return z, z, z, z, z, z, errors.Wrap(err, "invalid prediction in file\n")
			}

			if value == -1 {
				fmt.Printf("Trovato -1\n")
				// Trova la posizione corrente del file e salva
				lastReadPosition, _ = file.Seek(0, 1)

				break
			} else {
				// Aggiungi il numero alla lista delle predizioni
				predictions = append(predictions, value)
			}
		}
	}

	// Se non sono stati trovati almeno 10.000 numeri precedenti a -1
	fmt.Printf("Num trovati %d\n", len(predictions))
	if len(predictions) < 10000 {
		return z, z, z, z, z, z, nil
	}

	startTime := time.Now()
        fmt.Printf("[Observe ciclo] start at", startTime)
	trueLabels, err := loadTrueLabels()
	if err != nil {
		return z, z, z, z, z, z, errors.Wrap(err, "cannot load ground truth\n")
	}

	truePositive, falsePositive, falseNegative, trueNegative, total := calculateConfusionMatrix(predictions, trueLabels)

	endTime := time.Now()
    	fmt.Printf("[Observe ciclo] Duration: %v", endTime.Sub(startTime))

	newmetrics = 1
	return truePositive, falsePositive, falseNegative, trueNegative, total, newmetrics, nil
}

func loadTrueLabels() ([]int, error) {
	fmt.Printf("LoadTrueLabels\n")
	file, err := os.Open("/var/data/ml_model_prova/labels/labels.csv")
	if err != nil {
		return nil, errors.Wrap(err, "cannot open ground truth file\n")
	}
	defer func() {
		if cerr := file.Close(); cerr != nil {
			fmt.Printf("Error closing file: %v\n", cerr)
		}
	}()
	var trueLabels []int
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		value, err := strconv.Atoi(line)
		if err != nil {
			return nil, errors.Wrap(err, "invalid ground truth value\n")
		}
		trueLabels = append(trueLabels, value)
	}

	return trueLabels, nil
}

func calculateConfusionMatrix(predictedLabels []int, trueLabels []int) (int, int, int, int, int) {
	fmt.Printf("CalculateConfusionMatrix\n")
	var truePositive, falsePositive, falseNegative, trueNegative, total int
	var confusionMatrix [OUTPUT_SIZE][OUTPUT_SIZE]int
	var tp [10]int
	var fp [10]int
	var fn [10]int

	// Calcolo della matrice di confusione
	for i := 0; i < len(trueLabels); i++ {
		if trueLabels[i] < OUTPUT_SIZE && predictedLabels[i] < OUTPUT_SIZE {
			confusionMatrix[trueLabels[i]][predictedLabels[i]]++
		}
	}

	for i := 0; i < OUTPUT_SIZE; i++ {
		truePositive += confusionMatrix[i][i]
		tp[i] += confusionMatrix[i][i]
		for j := i; j < OUTPUT_SIZE; j++ {
			if j != i {
				falsePositive += confusionMatrix[j][i]
				fp[j] += confusionMatrix[j][i]
				falseNegative += confusionMatrix[i][j]
				fn[i] += confusionMatrix[i][j]
			}
		}
	}

	total = 10000
	trueNegative = total - falsePositive - falseNegative - truePositive

	var metrics1 Metrics

	for i := 0; i < OUTPUT_SIZE; i++ {
		acc := strconv.FormatFloat(float64(tp[i])/float64(tp[i]+fp[i]+fn[i]), 'f', -1, 64)
		errRate := strconv.FormatFloat(float64(fp[i]+fn[i])/float64(tp[i]+fp[i]+fn[i]), 'f', -1, 64)

		fmt.Printf("Calculated Metrics di questo ciclo per valore ", i, ": Accuracy= ", acc, ", ErrorRate= ", errRate)
		fmt.Printf("\n")
	}

	metrics1, r1 := calculateMetrics(truePositive, falsePositive, falseNegative, trueNegative, total)

	if r1 == 1 {
		fmt.Printf("Calculated Metrics di questo ciclo: ", "Precision", metrics1.Precision, "Recall", metrics1.Recall, "F1Score", metrics1.F1Score, "Total", total,
			   "Accuracy", metrics1.Accuracy, "ErrorRate", metrics1.ErrorRate, "TP", truePositive, "FP", falsePositive, "FN", falseNegative, "TN", trueNegative)
		fmt.Printf("\n")
	}

	checkMetrics(metrics1)

	return truePositive, falsePositive, falseNegative, trueNegative, total
}

func calculateMetrics(truePositive int, falsePositive int, falseNegative int, trueNegative int, total int) (Metrics, int) {
	res = 1

	precision := strconv.FormatFloat(float64(truePositive)/float64(truePositive+falsePositive), 'f', -1, 64)
	recall := strconv.FormatFloat(float64(truePositive)/float64(truePositive+falseNegative), 'f', -1, 64)
	f1Score := strconv.FormatFloat(2*((float64(truePositive)/float64(truePositive+falsePositive))*(float64(truePositive)/float64(truePositive+falseNegative)))/(float64(truePositive)/float64(truePositive+falsePositive)+float64(truePositive)/float64(truePositive+falseNegative)), 'f', -1, 64)
	accuracy := strconv.FormatFloat((float64(truePositive+trueNegative) / float64(total)), 'f', -1, 64)
	errorRate := strconv.FormatFloat(float64(falsePositive+falseNegative)/ float64(total), 'f', -1, 64)

	fmt.Printf("Calculated Metrics: ", "Precision", precision, "Recall", recall, "F1Score", f1Score, "Accuracy", accuracy, "ErrorRate", errorRate,
		   "TP", truePositive, "FP", falsePositive, "FN", falseNegative, "TN", trueNegative, "Total", total)
	fmt.Printf("\n")

	return Metrics{
		Precision: precision,
		Recall:    recall,
		F1Score:   f1Score,
		Accuracy:  accuracy,
		ErrorRate: errorRate,
	}, res
}

func checkMetrics(metrics Metrics) {

	precision, err := strconv.ParseFloat(metrics.Precision, 64)
	if err != nil {
        	fmt.Println("Errore nella conversione di Precision:\n", err)
        	return
    	}

	recall, err := strconv.ParseFloat(metrics.Recall, 64)
	if err != nil {
        	fmt.Println("Errore nella conversione di Recall:\n", err)
        	return
    	}

	f1Score, err := strconv.ParseFloat(metrics.F1Score, 64)
	if err != nil {
        	fmt.Println("Errore nella conversione di F1Score:\n", err)
        	return
	}

	accuracy, err := strconv.ParseFloat(metrics.Accuracy, 64)
	if err != nil {
        	fmt.Println("Errore nella conversione di Accuracy:\n", err)
        	return
    	}

	errorRate, err := strconv.ParseFloat(metrics.ErrorRate, 64)
	if err != nil {
        	fmt.Println("Errore nella conversione di ErrorRate:\n", err)
        	return
    	}

	if precision < 0.8 {
		fmt.Printf("ATTENZIONE! Metrica di precision calata sotto 80%, possibile problema! Precision: \n", metrics.Precision)
	}
	if recall < 0.8 {
		fmt.Printf("ATTENZIONE! Metrica di recall calata sotto 80%, possibile problema! Recall: \n", metrics.Recall)
	}
	if f1Score < 0.8 {
		fmt.Printf("ATTENZIONE! Metrica di f1Score calata sotto 80%, possibile problema! F1Score: \n", metrics.F1Score)
	}
	if accuracy < 0.8 {
		fmt.Printf("ATTENZIONE! Metrica di accuracy calata sotto 80%, possibile problema! Accuracy: \n", metrics.Accuracy)
	}
	if errorRate > 0.2 {
		fmt.Printf("ATTENZIONE! Metrica di ErrorRate aumentata sopra al 20%, possibile problema! ErrorRate: \n", metrics.ErrorRate)
	}
}

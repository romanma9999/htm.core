/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2018, Numenta, Inc.
 *               2018, chhenning
 *               2019, David McDougall
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * --------------------------------------------------------------------- */

/** @file
 * PyBind11 bindings for SDRClassifier class
 */

#include <bindings/suppress_register.hpp>  //include before pybind11.h
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <htm/algorithms/SDRClassifier.hpp>

namespace htm_ext
{
    namespace py = pybind11;
    using namespace std;
    using namespace htm;

    void init_SDR_Classifier(py::module& m)
    {
        py::class_<Classifier> py_Classifier(m, "Classifier",
R"(The SDR Classifier takes the form of a single layer classification network.
It accepts SDRs as input and outputs a predicted distribution of categories.

Categories are labeled using unsigned integers.  Other data types must be
enumerated or transformed into postitive integers.  There are as many output
units as the maximum category label.

Example Usage:
    # Make a random SDR and associate it with a category.
    inputData  = SDR( 1000 ).randomize( 0.02 )
    categories = { 'A': 0, 'B': 1, 'C': 2, 'D': 3 }
    clsr = Classifier()
    clsr.learn( inputData, categories['B'] )
    numpy.argmax( clsr.infer( inputData ) )  ->  categories['B']

    # Estimate a scalar value.  The Classifier only accepts categories, so
    # put real valued inputs into bins (AKA buckets) by subtracting the
    # minimum value and dividing by a resolution.
    scalar     = 567.8
    minimum    = 500
    resolution = 10
    clsr.learn( inputData, int((scalar - minimum) / resolution) )
    numpy.argmax( clsr.infer( inputData ) ) * resolution + minimum  ->  560

During inference, the output is calculated by first doing a weighted
summation of all the inputs, and then perform a softmax nonlinear function to
get the predicted distribution of category labels.

During learning, the connection weights between input units and output units
are adjusted to maximize the likelihood of the model.

References:
    - Alex Graves. Supervised Sequence Labeling with Recurrent Neural Networks,
     PhD Thesis, 2008
    - J. S. Bridle. Probabilistic interpretation of feedforward classification
     network outputs, with relationships to statistical pattern recognition
    - In F. Fogleman-Soulie and J.Herault, editors, Neurocomputing: Algorithms,
     Architectures and Applications, pp 227-236, Springer-Verlag, 1990)");

        py_Classifier.def(py::init<Real>(),
R"(Argument alpha is used to adapt the weight matrix during learning.
A larger alpha results in faster adaptation to the data.)",
            py::arg("alpha") = 0.001);

        py_Classifier.def("infer", &Classifier::infer,
R"(Compute the likelihoods for each category / bucket.

Argument pattern is the SDR containing the active input bits.

Returns the Probablility Distribution Function (PDF) of the categories.
The PDF is a list of probablilities which sums to 1.  Each index in this list is
a category label, and each value is the likelihood of the that category.
Use "numpy.argmax" to find the category with the greatest probablility.)",

            py::arg("pattern"));

        py_Classifier.def("learn", 
          static_cast<void (htm::Classifier::*)(const htm::SDR&, const std::vector<UInt>&)>(&Classifier::learn),
R"(Learn from example data.
Argument pattern is the SDR containing the active input bits.
Argument classification is the current category or bucket index.
This may also be a list for when the input has multiple categories.)",
                py::arg("pattern"),
                py::arg("classification"));

        py_Classifier.def("learn", 
          static_cast<void (htm::Classifier::*)(const htm::SDR&, UInt)>(&Classifier::learn),
                py::arg("pattern"),
                py::arg("classification"));
                
				// saving and loading from file
        py_Classifier.def("saveToFile",
         static_cast<void (htm::Classifier::*)(std::string, std::string) const>(&htm::Classifier::saveToFile), 
         py::arg("file"), py::arg("fmt") = "BINARY",
         "Serializes object to file. file: filename to write to.  fmt: format, one of 'BINARY', 'PORTABLE', 'JSON', or 'XML'.");

        py_Classifier.def("loadFromFile",    
         static_cast<void (htm::Classifier::*)(std::string, std::string)>(&htm::Classifier::loadFromFile), 
         py::arg("file"), py::arg("fmt") = "BINARY",
         "Deserializes object from file. file: filename to read from.  fmt: format recorded by saveToFile(). ");



        // Pickle support

        // pickle
        py_Classifier.def(py::pickle(
            [](const Classifier& self)
        {
            // __getstate__
            std::ostringstream os;

            self.save(os);

            return py::bytes(os.str());
        },
            [](const py::bytes &str)
        {
            // __setstate__
            if (py::len(str) == 0)
            {
                throw std::runtime_error("Empty state");
            }

            std::stringstream is( str.cast<std::string>() );

            std::unique_ptr<Classifier> clsr(new Classifier());
            clsr->load(is);

            return clsr;
        }
        ));


        py::class_<Predictor> py_Predictor(m, "Predictor",
R"(The Predictor class does N-Step ahead predictions.

Internally, this class uses Classifiers to associate SDRs with future values.
This class handles missing datapoints.

Compatibility Note:  This class is the replacement for the old SDRClassifier.
It no longer provides estimates of the actual value.

Example Usage:
    # Predict 1 and 2 time steps into the future.

    # Make a sequence of 4 random SDRs, each SDR has 1000 bits and 2% sparsity.
    sequence = [ SDR( 1000 ).randomize( 0.02 ) for i in range(4) ]

    # Make category labels for the sequence.
    labels = [ 4, 5, 6, 7 ]

    # Make a Predictor and train it.
    pred = Predictor([ 1, 2 ])
    pred.learn( 0, sequence[0], labels[0] )
    pred.learn( 1, sequence[1], labels[1] )
    pred.learn( 2, sequence[2], labels[2] )
    pred.learn( 3, sequence[3], labels[3] )

    # Give the predictor partial information, and make predictions
    # about the future.
    pred.reset()
    A = pred.infer( sequence[0] )
    numpy.argmax( A[1] )  ->  labels[1]
    numpy.argmax( A[2] )  ->  labels[2]

    B = pred.infer( sequence[1] )
    numpy.argmax( B[1] )  ->  labels[2]
    numpy.argmax( B[2] )  ->  labels[3]
)");

        py_Predictor.def(py::init<const std::vector<UInt> &, Real>(),
R"(Argument steps is the number of steps into the future to learn and predict.
The Predictor accepts a list of steps.

Argument alpha is used to adapt the weight matrix during learning.
A larger alpha results in faster adaptation to the data.)",
            py::arg("steps"),
            py::arg("alpha") = 0.001);

        py_Predictor.def("reset", &Predictor::reset,
R"(For use with time series datasets.)");

        py_Predictor.def("infer", &Predictor::infer,
R"(Compute the likelihoods.

Argument pattern is the SDR containing the active input bits.

Returns a dictionary whos keys are prediction steps, and values are PDFs.
See help(Classifier.infer) for details about PDFs.)",
            py::arg("pattern"));

        py_Predictor.def("learn", static_cast<void (htm::Predictor::*)(UInt, const htm::SDR&, const std::vector<UInt>&)>(&Predictor::learn),
R"(Learn from example data.

Argument recordNum is an incrementing integer for each record.
Gaps in numbers correspond to missing records.

Argument pattern is the SDR containing the active input bits.

Argument classification is the current category or bucket index.
This may also be a list for when the input has multiple categories.)",
            py::arg("recordNum"),
            py::arg("pattern"),
            py::arg("classification"));

        py_Predictor.def("learn", static_cast<void (htm::Predictor::*)(UInt, const htm::SDR&, UInt)>(&Predictor::learn),
                py::arg("recordNum"),
                py::arg("pattern"),
                py::arg("classification"));

       py_Predictor.def("saveToFile",
         static_cast<void (htm::Predictor::*)(std::string, std::string) const>(&htm::Predictor::saveToFile), 
         py::arg("file"), py::arg("fmt") = "BINARY",
         R"(Serializes object to file. file: filename to write to.  fmt: format, one of 'BINARY', 'PORTABLE', 'JSON', or 'XML')");

       py_Predictor.def("loadFromFile",    
         static_cast<void (htm::Predictor::*)(std::string, std::string)>(&htm::Predictor::loadFromFile), 
         py::arg("file"), py::arg("fmt") = "BINARY",
         R"(Deserializes object from file. file: filename to read from.  fmt: format recorded by saveToFile(). )");

        // Pickle support

        // pickle
        py_Predictor.def(py::pickle(
            [](const Predictor& self)
        {
            // __getstate__
            std::ostringstream os;

            self.save(os);

            return py::bytes(os.str());
        },
            [](const py::bytes &str)
        {
            // __setstate__
            if (py::len(str) == 0)
            {
                throw std::runtime_error("Empty state");
            }

            std::stringstream is( str.cast<std::string>() );

            std::unique_ptr<Predictor> pred(new Predictor());
            pred->load(is);

            return pred;
        }
        ));
    }
} // namespace htm_ext

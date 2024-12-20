#include "veins/modules/application/traci/TraCIDemo11p.h"
#include "veins/modules/application/traci/TraCIDemo11pMessage_m.h"
#include <chrono>
#include <thread>

using namespace veins;

Define_Module(veins::TraCIDemo11p);

void TraCIDemo11p::initialize(int stage) {
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        sentMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;

        // Initialize counters for accepted and rejected reports
        acceptedReportCount = 0;
        rejectedReportCount = 0;

        // Initialize validation report flag
        validationReportCount = false;

        int vehicleId = getParentModule()->getIndex();
        assignedRSU = (vehicleId % 2 == 0) ? "rsu[1]" : "rsu[0]";

        // Group Signature - Group Label Assignment
        std::string groupLabel = (vehicleId % 2 == 0) ? "Group2" : "Group1";
        getParentModule()->getDisplayString().setTagArg("t", 0, groupLabel.c_str());

        std::string rsuPath = "RSUExampleScenario." + assignedRSU + ".appl";
        RSUApp = dynamic_cast<TraCIDemoRSU11p*>(getModuleByPath(rsuPath.c_str()));

        if (RSUApp) {
            RSUApp->registerVehicle(mobility->getExternalId());
        } else {
            EV << "Error: RSU application not found at path " << rsuPath << endl;
        }

        scheduleAt(simTime() + 10, new cMessage("sendToRSU"));
    }
}

void TraCIDemo11p::onWSA(DemoServiceAdvertisment* wsa) {
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
        currentSubscribedServiceId = wsa->getPsid();
        if (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService(static_cast<Channel>(wsa->getTargetChannel()), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }
}

void TraCIDemo11p::onWSM(BaseFrame1609_4* frame) {
    TraCIDemo11pMessage* wsm = check_and_cast<TraCIDemo11pMessage*>(frame);
    std::string groupInfo = (getParentModule()->getIndex() % 2 == 0) ? "Group2" : "Group1";

    if (std::string(wsm->getDemoData()) == "validationReport") {
        auto startVerification = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 2; i++) {
            auto startCommunication = std::chrono::high_resolution_clock::now();
            auto startComputation = std::chrono::high_resolution_clock::now();

            // Group Signature - Validation of Group Member Report
            TA->received_validated_report_from_RSU(groupInfo);

            auto endComputation = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> computationalOverhead = endComputation - startComputation;
            EV << "Computational Overhead for Validation Report " << (i + 1)
               << " from " << groupInfo << ": " << computationalOverhead.count() << " ms" << endl;

            totalComputationalOverhead += computationalOverhead.count();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto endCommunication = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> communicationOverhead = endCommunication - startCommunication;
            EV << "Communication Overhead for Validation Report " << (i + 1)
               << " from " << groupInfo << ": " << communicationOverhead.count() << " ms" << endl;

            totalCommunicationOverhead += communicationOverhead.count();
        }

        auto endVerification = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> signatureVerificationTime = endVerification - startVerification;

        // Ensure signature verification time is recorded
        if (signatureVerificationTime.count() > 0) {
            totalSignatureVerificationTime += signatureVerificationTime.count();
            EV << "Total Signature Verification Time (validate report): " << signatureVerificationTime.count() << " ms" << endl;
        } else {
            EV_WARN << "" << getParentModule()->getIndex() << ".\n";
        }

        bool validationResult = RSUApp->validate_report_request();
        if (validationResult) {
            EV << "Validation report accepted for further processing from " << groupInfo << ".\n";
            acceptedReportCount++;
        } else {
            EV << "Validation report rejected by RSU from " << groupInfo << ".\n";
            rejectedReportCount++;
        }
    }

    if (wsm->getRecipientAddress() == myId) {
        findHost()->getDisplayString().setTagArg("i", 1, "green");
        EV << "Received response from RSU/Server: " << wsm->getDemoData() << " for group " << groupInfo << endl;
    }
}

void TraCIDemo11p::handleSelfMsg(cMessage* msg) {
    int vehicleId = getParentModule()->getIndex();

    if (msg->isName("sendToRSU")) {
        if (!sentMessage && !validationInProgress) {
            if (vehicleId == 0 || vehicleId == 7) {
                sendViolationReport();
            } else {
                sendValidationReport();
            }
        }
        scheduleAt(simTime() + 10, msg);
    } else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void TraCIDemo11p::sendViolationReport() {
    if (!RSUApp) return;

    int vehicleId = getParentModule()->getIndex();
    if (vehicleId != 0 && vehicleId != 7) return;

    TraCIDemo11pMessage* wsm = new TraCIDemo11pMessage();
    populateWSM(wsm);
    wsm->setDemoData("violationReport");
    wsm->setRecipientAddress(-1); // Broadcast to RSU

    EV_INFO << "* Reporting Violation *\n";
    EV_INFO << "Vehicle ID: " << mobility->getExternalId() << "\n";

    // Simulate and record overhead metrics for the violation report
    auto startComputation = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Simulate computation
    auto endComputation = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> computationalOverhead = endComputation - startComputation;
    totalComputationalOverhead += computationalOverhead.count();

    auto startCommunication = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Simulate communication
    auto endCommunication = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> communicationOverhead = endCommunication - startCommunication;
    totalCommunicationOverhead += communicationOverhead.count();

    // Simulate signature validation time
    auto startValidation = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Simulate validation delay
    auto endValidation = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> signatureValidationTime = endValidation - startValidation;
    totalSignatureVerificationTime += signatureValidationTime.count();

    EV_INFO << "Violation report metrics for node[" << vehicleId << "]:\n";
    EV_INFO << "Computational Overhead: " << computationalOverhead.count() << " ms\n";
    EV_INFO << "Communication Overhead: " << communicationOverhead.count() << " ms\n";
    EV_INFO << "Signature Validation Time: " << signatureValidationTime.count() << " ms\n";

    RSUApp->authenticate_reporter(mobility->getExternalId());
    sentMessage = true;
    validationInProgress = true;

    violationReportCount++;
    EV_INFO << "Violation report sent by node[" << vehicleId << "].\n";

    sendDown(wsm);
    bool ok=RSUApp->validate_report_request();
}

void TraCIDemo11p::sendValidationReport() {
    int vehicleId = getParentModule()->getIndex();
    if (vehicleId == 0 || vehicleId == 7) return; // Skip vehicles sending violation reports

    if (validationReportCount) {
        EV_WARN << "Validation report already sent by node[" << vehicleId << "]. Skipping.\n";
        return;
    }

    TraCIDemo11pMessage* validationReport = new TraCIDemo11pMessage();
    populateWSM(validationReport);
    validationReport->setDemoData("validationReport");
    validationReport->setRecipientAddress(-1); // Broadcast to RSU

    // Simulate Computational Overhead
    auto startComputation = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto endComputation = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> computationalOverhead = endComputation - startComputation;
    totalComputationalOverhead += computationalOverhead.count();

    // Simulate Communication Overhead
    auto startCommunication = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto endCommunication = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> communicationOverhead = endCommunication - startCommunication;
    totalCommunicationOverhead += communicationOverhead.count();

    // Simulate Signature Validation Time
    auto startValidation = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Simulate validation delay
    auto endValidation = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> signatureValidationTime = endValidation - startValidation;
    totalSignatureVerificationTime += signatureValidationTime.count();

    validationReportCount++;

    EV_INFO << "Validation report metrics for node[" << vehicleId << "]:\n";
    EV_INFO << "Computational Overhead: " << computationalOverhead.count() << " ms\n";
    EV_INFO << "Communication Overhead: " << communicationOverhead.count() << " ms\n";
    EV_INFO << "Signature Validation Time: " << signatureValidationTime.count() << " ms\n";

    sendDown(validationReport);
}

void TraCIDemo11p::handlePositionUpdate(cObject* obj) {
    DemoBaseApplLayer::handlePositionUpdate(obj);

    if (mobility->getSpeed() < 1) {
        if (simTime() - lastDroveAt >= 10 && !sentMessage) {
            findHost()->getDisplayString().setTagArg("i", 1, "red");
            sentMessage = true;
        }
    } else {
        lastDroveAt = simTime();
    }

    EV_INFO << "Vehicle " << getParentModule()->getIndex()
            << " is active at position: " << mobility->getCurrentDirection() << "\n";
}

void TraCIDemo11p::finish() {
    recordScalar("Total Violation Reports Sent", violationReportCount);
    recordScalar("Total Validation Reports Sent", validationReportCount);
    recordScalar("Total Accepted Reports", acceptedReportCount);
    recordScalar("Total Rejected Reports", rejectedReportCount);
    recordScalar("Total Computational Overhead (ms)", totalComputationalOverhead);
    recordScalar("Total Communication Overhead (ms)", totalCommunicationOverhead);
    recordScalar("Total Signature Verification Time (ms)", totalSignatureVerificationTime);
}

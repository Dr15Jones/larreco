
art_make( 
          LIB_LIBRARIES larsim_MCCheater_BackTracker_service
                        lardata_RecoBase
                        lardata_RecoBaseArt
                        larsim_Simulation
                        larevt_Filters
                        larcore_Geometry
                        larcore_Geometry_Geometry_service
                        lardata_RecoObjects
                        lardata_AnalysisBase
			lardata_AnalysisAlg
                        lardata_Utilities
                        larreco_RecoAlg_ClusterRecoUtil
			larreco_RecoAlg_CMTool_CMToolBase
			larreco_RecoAlg_CMTool_CMTAlgMerge
			larreco_RecoAlg_CMTool_CMTAlgPriority
			larreco_RecoAlg_CMTool_CMTAlgMatch
			larreco_RecoAlg_CMTool_CMToolApp
      larreco_RecoAlg_TCAlg
			larreco_RecoAlg_PMAlg
                        ${SIMULATIONBASE}
                        ${ART_FRAMEWORK_CORE}
			${ART_FRAMEWORK_PRINCIPAL}
			${ART_FRAMEWORK_SERVICES_REGISTRY}
			${ART_FRAMEWORK_SERVICES_BASIC}
			${ART_FRAMEWORK_SERVICES_OPTIONAL}
			${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
                        ${ART_FRAMEWORK_SERVICES_OPTIONAL_RANDOMNUMBERGENERATOR_SERVICE}
			${ART_PERSISTENCY_COMMON}
			${ART_PERSISTENCY_PROVENANCE}
			${ART_UTILITIES}
			${MF_MESSAGELOGGER}
			${MF_UTILITIES}
	                ${FHICLCPP}
			${CETLIB}
                        ${CLHEP}
			${ROOT_BASIC_LIB_LIST}
                        ${ROOT_MINUIT}
			${ROOT_MINUIT2}	
        )

add_subdirectory(CMTool)
add_subdirectory(ClusterRecoUtil)
add_subdirectory(Cluster3DAlgs)
add_subdirectory(PMAlg)
add_subdirectory(TCAlg)

install_headers()
install_fhicl()
install_source()


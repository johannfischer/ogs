/**
 * \file
 * \copyright
 * Copyright (c) 2012-2019, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "RichardsFlowProcess.h"

#include <cassert>

#include "ProcessLib/Utils/CreateLocalAssemblers.h"

namespace ProcessLib
{
namespace RichardsFlow
{
RichardsFlowProcess::RichardsFlowProcess(
    std::string name,
    MeshLib::Mesh& mesh,
    std::unique_ptr<ProcessLib::AbstractJacobianAssembler>&& jacobian_assembler,
    std::vector<std::unique_ptr<ParameterLib::ParameterBase>> const& parameters,
    unsigned const integration_order,
    std::vector<std::vector<std::reference_wrapper<ProcessVariable>>>&&
        process_variables,
    RichardsFlowProcessData&& process_data,
    SecondaryVariableCollection&& secondary_variables,
    NumLib::NamedFunctionCaller&& named_function_caller,
    BaseLib::ConfigTree const& /*config*/,
    std::map<std::string,
             std::unique_ptr<MathLib::PiecewiseLinearInterpolation>> const&
    /*curves*/)
    : Process(std::move(name), mesh, std::move(jacobian_assembler), parameters,
              integration_order, std::move(process_variables),
              std::move(secondary_variables), std::move(named_function_caller)),
      _process_data(std::move(process_data))
{
}

void RichardsFlowProcess::initializeConcreteProcess(
    NumLib::LocalToGlobalIndexMap const& dof_table,
    MeshLib::Mesh const& mesh,
    unsigned const integration_order)
{
    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];
    ProcessLib::createLocalAssemblers<LocalAssemblerData>(
        mesh.getDimension(), mesh.getElements(), dof_table,
        pv.getShapeFunctionOrder(), _local_assemblers,
        mesh.isAxiallySymmetric(), integration_order, _process_data);

    _secondary_variables.addSecondaryVariable(
        "saturation",
        makeExtrapolator(
            1, getExtrapolator(), _local_assemblers,
            &RichardsFlowLocalAssemblerInterface::getIntPtSaturation));

    _secondary_variables.addSecondaryVariable(
        "darcy_velocity",
        makeExtrapolator(
            mesh.getDimension(), getExtrapolator(), _local_assemblers,
            &RichardsFlowLocalAssemblerInterface::getIntPtDarcyVelocity));
}

void RichardsFlowProcess::assembleConcreteProcess(
    const double t, double const dt, GlobalVector const& x, GlobalMatrix& M,
    GlobalMatrix& K, GlobalVector& b)
{
    DBUG("Assemble RichardsFlowProcess.");

    std::vector<std::reference_wrapper<NumLib::LocalToGlobalIndexMap>>
       dof_table = {std::ref(*_local_to_global_index_map)};
    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];

    // Call global assembler for each local assembly item.
    GlobalExecutor::executeSelectedMemberDereferenced(
        _global_assembler, &VectorMatrixAssembler::assemble, _local_assemblers,
        pv.getActiveElementIDs(), dof_table, t, dt, x, M, K, b,
        _coupled_solutions);
}

void RichardsFlowProcess::assembleWithJacobianConcreteProcess(
    const double t, double const dt, GlobalVector const& x,
    GlobalVector const& xdot, const double dxdot_dx, const double dx_dx,
    GlobalMatrix& M, GlobalMatrix& K, GlobalVector& b, GlobalMatrix& Jac)
{
    DBUG("AssembleWithJacobian RichardsFlowProcess.");

    std::vector<std::reference_wrapper<NumLib::LocalToGlobalIndexMap>>
       dof_table = {std::ref(*_local_to_global_index_map)};
    const int process_id = 0;
    ProcessLib::ProcessVariable const& pv = getProcessVariables(process_id)[0];

    // Call global assembler for each local assembly item.
    GlobalExecutor::executeSelectedMemberDereferenced(
        _global_assembler, &VectorMatrixAssembler::assembleWithJacobian,
        _local_assemblers, pv.getActiveElementIDs(), dof_table, t, dt, x, xdot,
        dxdot_dx, dx_dx, M, K, b, Jac, _coupled_solutions);
}

}  // namespace RichardsFlow
}  // namespace ProcessLib

from client_helper.models.constants import (
    AccumlationBehaviourType, CommodityType, FlowDirectionType,
    KindType, UomType
)
from client_helper.models import ReadingType

class TypeConstants(object):

    ACTIVE_POWER = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Forward,       
        kind=KindType.Power,
        uom=UomType.W,      
        powerOfTenMultiplier=0,   
    )
    
    REACTIVE_POWER = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Forward,       
        kind=KindType.Power,
        uom=UomType.VAr,   
        powerOfTenMultiplier=0,
    )
    
    APPARENT_POWER = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Forward,       
        kind=KindType.Power,
        uom=UomType.VA,    
        powerOfTenMultiplier=0,
    )
    
    VOLTAGE = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Not_applicable,
        kind=KindType.Not_applicable,
        uom=UomType.Voltage,        
        powerOfTenMultiplier=0,   
    )
    
    CURRENT = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Forward,       
        kind=KindType.Not_applicable,
        uom=UomType.Amperes,        
        powerOfTenMultiplier=-3,  
    )

    FREQUENCY = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,  
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Not_applicable,
        kind=KindType.Not_applicable,
        uom=UomType.Hz,    
        powerOfTenMultiplier=-2,  
    )
    
    ENERGY_EXPORTED = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Summation,      
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Forward,       
        kind=KindType.Energy,        
        uom=UomType.Wh,    
        powerOfTenMultiplier=0,
    )

    PERCENTAGE = ReadingType(
        accumulationBehaviour=AccumlationBehaviourType.Instantaneous,
        commodity=CommodityType.Electricity_secondary_metered,
        flowDirection=FlowDirectionType.Not_applicable,
        kind=KindType.Not_applicable,
        uom=UomType.Not_applicable,
        powerOfTenMultiplier=2,
    )
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional
from .sep import (
    DER,
    IdentifiedObject,
    Link,
    Resource,
    SubscribableList,
)

__NAMESPACE__ = "epri:derforecast:ns"


@dataclass
class ForecastNumericType:
    """
    Real number expressed as an integer and power-of-ten-multiplier.

    :ivar value: Value expressed as integer
    :ivar multiplier: Multiplier for value. Multiply value by 10^this.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    value: Optional[int] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )
    multiplier: Optional[int] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )


@dataclass
class DERForecastLink(Link):
    """
    SHALL contain a Link to an instance of DERForecast.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    postRate: int = field(
        default=900,
        metadata={
            "type": "Attribute",
        }
    )


@dataclass
class ForecastParameter:
    """
    Object holding forecast for a single parameter.

    :ivar name: Name of the paramater
    :ivar forecast: Forecast for the parameter named.
    :ivar sigma: Standard deviation for the parameter named.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    name: Optional[int] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )
    forecast: Optional[ForecastNumericType] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )
    sigma: Optional[ForecastNumericType] = field(
        default=None,
        metadata={
            "type": "Element",
        }
    )


@dataclass
class DERFlexibility(DER):
    """
    Extends sep DER to include DERForecastLink.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    DERForecastLink: Optional[DERForecastLink] = field(
        default=None,
        metadata={
            "type": "Element",
        }
    )


@dataclass
class ForecastParameterSet(Resource):
    """
    A set of forecasts.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    ForecastParameter: List[ForecastParameter] = field(
        default_factory=list,
        metadata={
            "type": "Element",
        }
    )


@dataclass
class ForecastParameterSetList(SubscribableList):
    """
    A List element to hold ForecastParameterSet  objects.
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    ForecastParameterSet: List[ForecastParameterSet] = field(
        default_factory=list,
        metadata={
            "type": "Element",
        }
    )


@dataclass
class DERForecast(IdentifiedObject):
    """
    DER forecast information.

    :ivar startTime: The start time in epoch for this forecast.
    :ivar interval: Forecast interval for the included
        ForecastParameterSetList, in seconds.
    :ivar ForecastParameterSetList:
    """
    class Meta:
        namespace = "epri:derforecast:ns"

    startTime: Optional[int] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )
    interval: Optional[int] = field(
        default=None,
        metadata={
            "type": "Element",
            "required": True,
        }
    )
    ForecastParameterSetList: Optional[ForecastParameterSetList] = field(
        default=None,
        metadata={
            "type": "Element",
        }
    )

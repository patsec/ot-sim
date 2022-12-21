'''
<ot-sim>
  <wind-turbine>
    <power-output>
      <turbine-type>E-126/4200</turbine-type>
      <hub-height>135</hub-height>
      <roughness-length>0.15</roughness-length>
      <weather-data>
        <column name="wind_speed"  height="58.2">speed.high</column>
        <column name="temperature" height="58.2">temp.high</column>
        <column name="wind_speed"  height="36.6">speed.med</column>
        <column name="wind_speed"  height="15.0">speed.low</column>
        <column name="temperature" height="15.0">temp.low</column>
        <column name="pressure"    height="0">pressure</column>
      </weather-data>
      <tags>
        <cut-in>turbine.cut-in</cut-in>
        <cut-out>turbine.cut-out</cut-out>
        <output>turbine.mw-output</output>
        <emergency-stop>turbine.emergency-stop</emergency-stop>
      </tags>
    </power-output>
  </wind-turbine>
</ot-sim>
'''
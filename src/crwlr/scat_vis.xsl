<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html"/>
<xsl:template match="/site">
<html>
	<head>
    <!--script type="text/javascript" src="https://www.google.com/jsapi"></script-->
    <script type="text/javascript" src="file:///home/tigrang/projects/tbandit/crwlr/jsapi"></script>
    <script type="text/javascript">
		<xsl:text>
      google.load("visualization", "1", {packages:["corechart"]});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = new google.visualization.DataTable();
				data.addColumn('number');
				data.addColumn('number');
				data.addColumn('number');
				data.addRows([
		</xsl:text>
	<xsl:for-each select="url">
		<xsl:if test="position()!=1">
			<xsl:text>,</xsl:text>
		</xsl:if>
		<xsl:text>[</xsl:text>
		<xsl:value-of select="@mean"/>
		<xsl:text> , </xsl:text>
		<xsl:choose>
			<xsl:when test='@schema="http"'>
				<xsl:value-of select="@sdev"/>
				<xsl:text>, null </xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>null,</xsl:text>
				<xsl:value-of select="@sdev"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>]&#x0a;</xsl:text>
	</xsl:for-each>
  <xsl:text>
      ]);
  </xsl:text>
  <xsl:choose>
  <xsl:when test="url[1]/@measure='time'">
		<xsl:text>
        var options = {
          title: 'Average Time  Standard Deviation',
          hAxis: {title: 'Average Time in Seconds' , gridlines:{count:3}},
          vAxis: {title: 'Standard Deviation' , gridlines:{count:3}},
          legend: 'none'
        };
    </xsl:text>
  </xsl:when>
  <xsl:otherwise>
		<xsl:text>
        var options = {
          title: 'Average Speed  Standard Deviation',
          hAxis: {title: 'Average Speed in Kb/Sec' , gridlines:{count:3}},
          vAxis: {title: 'Standard Deviation' , gridlines:{count:3}},
          legend: 'none'
        };
    </xsl:text>
  </xsl:otherwise>
  </xsl:choose>
  <xsl:text>
      var chart = new google.visualization.ScatterChart(document.getElementById('chart_div'));
      chart.draw(data, options);
    }
  </xsl:text>
    </script>
  </head>
  <body>
    <div id="chart_div" style="width: 900px; height: 500px;"></div>
  </body>
</html>
</xsl:template>
</xsl:stylesheet>
